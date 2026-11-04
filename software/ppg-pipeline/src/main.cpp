#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include "DS18B20.hpp"
#include "PPGBuffer.hpp"
#include "PPGDebug.hpp"
#include "PPGFilter.hpp"
#include "PPGHeartRate.hpp"
#include "PPGQuality.hpp"
#include "PPGRespiratoryRate.hpp"
#include "PPGSource.hpp"
#include "PPGSpO2.hpp"

int main() {
    if (auto temp = get_temp()) {
        std::cout << *temp << '\n';
    } else {
        std::cerr << "get_temp: falha na leitura da temperatura\n";
    }

    PPGSourceInfo source_info = open_ppg_source();
    if (source_info.sample_rate_hz <= 0) {
        std::cerr << "main: nao foi possivel determinar a taxa de amostragem da fonte, abortando\n";
        return 1;
    }

    bool debug_dump_enabled = std::getenv("PPG_DEBUG_DUMP") != nullptr;
    std::ofstream debug_file;
    std::ofstream window_file;
    if (debug_dump_enabled) {
        const char* dump_path_override = std::getenv("PPG_DEBUG_DUMP_PATH");
        debug_file = open_ppg_debug_dump(dump_path_override ? dump_path_override : "../debug_output/ppg_full_trace.csv");

        const char* window_path_override = std::getenv("PPG_DEBUG_WINDOWS_PATH");
        window_file = open_ppg_window_dump(window_path_override ? window_path_override : "../debug_output/ppg_windows.csv");
    }

    bool param_export_enabled = std::getenv("PPG_PARAM_EXPORT") != nullptr;
    std::ofstream param_export_file;
    if (param_export_enabled) {
        const char* param_export_path_override = std::getenv("PPG_PARAM_EXPORT_PATH");
        param_export_file = open_ppg_parameters_export(param_export_path_override ? param_export_path_override : "../debug_output/ppg_parameters.csv");
    }

    PPGBandpassFilter red_filter(PPG_FILTER_LOW_CUTOFF_HZ, PPG_FILTER_HIGH_CUTOFF_HZ, source_info.sample_rate_hz);
    PPGBandpassFilter ir_filter(PPG_FILTER_LOW_CUTOFF_HZ, PPG_FILTER_HIGH_CUTOFF_HZ, source_info.sample_rate_hz);

    PPGBuffer ppg_buffer = make_ppg_buffer(source_info.sample_rate_hz);
    size_t hr_spo2_window_samples = seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, source_info.sample_rate_hz);
    int window_count = 0;
    int discarded_count = 0;
    size_t sample_index = 0;

    while (true) {
        auto sample = read_ppg_sample();
        if (!sample) {
            std::cerr << "read_ppg_sample: fim dos dados\n";
            break;
        }

        double red_filtered = red_filter.process(sample->red);
        double ir_filtered = ir_filter.process(sample->ir);

        if (debug_dump_enabled) {
            write_ppg_debug_sample(debug_file, sample_index, sample->red, red_filtered, sample->ir, ir_filtered);
        }
        sample_index++;

        PPGWindowSample window_sample{sample->red, sample->ir, red_filtered, ir_filtered};
        if (ppg_buffer_push(ppg_buffer, window_sample)) {
            window_count++;

            PPGWindowView hr_spo2_window = last_window(ppg_buffer, hr_spo2_window_samples);
            PPGWindowView rr_window = last_window(ppg_buffer, ppg_buffer.buffer_capacity);

            PPGQualityResult quality = assess_window_quality(hr_spo2_window, rr_window, source_info.raw_intensity);

            PPGPerfusionIndex pi_30s = calculate_perfusion_index(hr_spo2_window);
            PPGPerfusionIndex pi_60s = calculate_perfusion_index(rr_window);

            bool usable = quality.usable_for_hr;
            if (!usable) {
                discarded_count++;
            }
            std::cout << "janela " << window_count << (usable ? " aceita" : " descartada (qualidade)") << '\n';

            size_t end_sample = sample_index - 1;
            size_t start_sample = end_sample - hr_spo2_window_samples + 1;

            if (debug_dump_enabled) {
                write_ppg_window_record(window_file, window_count, start_sample, end_sample, usable);
            }

            std::string hr_status = "descartada";
            std::optional<double> heart_rate_bpm;
            std::optional<size_t> peak_count;
            if (usable) {
                auto heart_rate = calculate_heart_rate(hr_spo2_window, source_info.sample_rate_hz);
                heart_rate_bpm = heart_rate.bpm;
                peak_count = heart_rate.peak_count;
                if (heart_rate_bpm) {
                    hr_status = "aceita";
                    std::cout << "  FC: " << *heart_rate_bpm << " bpm\n";
                } else {
                    hr_status = "aceita_sem_fc";
                    std::cout << "  FC: não foi possível calcular\n";
                }
            }

            std::string spo2_status = "descartada";
            std::optional<double> spo2_value;
            if (quality.usable_for_spo2 && source_info.has_red_channel) {
                spo2_value = calculate_spo2(hr_spo2_window);
                if (spo2_value) {
                    spo2_status = "aceita";
                    std::cout << "  SpO2: " << *spo2_value << " %\n";
                } else {
                    spo2_status = "aceita_sem_spo2";
                    std::cout << "  SpO2: não foi possível calcular\n";
                }
            }

            std::string rr_status = "descartada";
            std::string rr_amplitude_status = "descartada";
            std::string rr_frequency_status = "descartada";
            std::string rr_baseline_status = "descartada";
            std::optional<double> rr_value;
            std::optional<double> rr_amplitude_value;
            std::optional<double> rr_frequency_value;
            std::optional<double> rr_baseline_value;
            if (quality.usable_for_rr) {
                PPGRespiratoryRateEstimates rr_estimates = calculate_respiratory_rate(rr_window, source_info.sample_rate_hz);
                rr_amplitude_value = rr_estimates.amplitude_rpm;
                rr_frequency_value = rr_estimates.frequency_rpm;
                rr_baseline_value = rr_estimates.baseline_rpm;
                rr_value = rr_estimates.fused_rpm;

                if (rr_estimates.pulse_period_artifact) {
                    rr_status = "artefato_periodo_pulso";
                    rr_amplitude_status = "artefato_periodo_pulso";
                    rr_frequency_status = "artefato_periodo_pulso";
                    rr_baseline_status = "artefato_periodo_pulso";
                    std::cout << "  FR: artefato de periodo de pulso\n";
                } else {
                    rr_amplitude_status = rr_amplitude_value ? "aceita" : "aceita_sem_rr";
                    rr_frequency_status = rr_frequency_value ? "aceita" : "aceita_sem_rr";
                    rr_baseline_status = rr_baseline_value ? "aceita" : "aceita_sem_rr";

                    if (rr_value) {
                        rr_status = "aceita";
                        std::cout << "  FR (fundida): " << *rr_value << " rpm\n";
                    } else if (!rr_amplitude_value || !rr_frequency_value || !rr_baseline_value) {
                        rr_status = "estimativa_ausente";
                        std::cout << "  FR (fundida): estimativa ausente em alguma modulacao\n";
                    } else {
                        rr_status = "discordancia";
                        std::cout << "  FR (fundida): discordancia entre modulacoes\n";
                    }

                    if (rr_amplitude_value) {
                        std::cout << "  FR (amplitude): " << *rr_amplitude_value << " rpm\n";
                    }
                    if (rr_frequency_value) {
                        std::cout << "  FR (frequencia): " << *rr_frequency_value << " rpm\n";
                    }
                    if (rr_baseline_value) {
                        std::cout << "  FR (linha de base): " << *rr_baseline_value << " rpm\n";
                    }
                }
            }

            if (param_export_enabled) {
                double start_time_s = static_cast<double>(start_sample) / source_info.sample_rate_hz;
                double end_time_s = static_cast<double>(end_sample + 1) / source_info.sample_rate_hz;
                PPGParameterRecord record{start_time_s,      end_time_s,          hr_status,
                                           heart_rate_bpm,    peak_count,         spo2_status,
                                           spo2_value,        rr_status,          rr_value,
                                           rr_amplitude_status, rr_amplitude_value,
                                           rr_frequency_status, rr_frequency_value, rr_baseline_status,
                                           rr_baseline_value,
                                           pi_30s.red,        pi_30s.ir,          pi_60s.ir};
                write_ppg_parameters_record(param_export_file, record);
            }
        }
    }

    std::cout << "janelas descartadas: " << discarded_count << " de " << window_count << '\n';

    return 0;
}
