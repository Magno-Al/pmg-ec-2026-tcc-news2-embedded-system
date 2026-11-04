#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include "PPG.hpp"
#include "PPGBuffer.hpp"
#include "PPGRespiratoryRate.hpp"
#include "PPGRespiratorySignal.hpp"

namespace {

constexpr double PI_CONST = 3.14159265358979323846;
constexpr double TOLERANCE_RPM = 3.0; // FR é inerentemente menos precisa que FC/SpO2

bool check_result(const std::optional<double>& result, double expected_rpm, const char* label) {
    if (!result) {
        std::cerr << "FALHOU (" << label << "): nao foi possivel calcular FR para sinal sintetico de "
                  << expected_rpm << " rpm\n";
        return false;
    }

    double error = std::abs(*result - expected_rpm);
    if (error > TOLERANCE_RPM) {
        std::cerr << "FALHOU (" << label << "): FR calculada " << *result << " rpm, esperado " << expected_rpm
                  << " rpm (erro " << error << " > " << TOLERANCE_RPM << ")\n";
        return false;
    }

    std::cout << "OK (" << label << "): FR calculada " << *result << " rpm (esperado " << expected_rpm << " rpm)\n";
    return true;
}

// Batimentos cardíacos (canal IR) com AMPLITUDE modulada numa FR conhecida.
bool check_amplitude_modulation() {
    double expected_rpm = 15.0;
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = expected_rpm / 60.0;
    double modulation_depth = 0.3;

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double envelope = 1.0 + modulation_depth * std::sin(2.0 * PI_CONST * respiratory_hz * t);
        double value = envelope * std::sin(2.0 * PI_CONST * heart_hz * t);
        buffer.samples[i] = PPGWindowSample{0, 0, 0.0, value};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);
    return check_result(estimates.amplitude_rpm, expected_rpm, "amplitude");
}

// Intervalo entre batimentos oscilando na FR (arritmia sinusal respiratória):
// fase do batimento modulada em frequência, amplitude constante de propósito.
bool check_frequency_modulation() {
    double expected_rpm = 15.0;
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = expected_rpm / 60.0;
    double frequency_modulation_depth_hz = 0.15; // variação na frequência instantânea do batimento

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        // fase(t) tal que dfase/dt = 2*pi*(heart_hz + freq_mod_depth*sin(2*pi*respiratory_hz*t))
        double phase = 2.0 * PI_CONST * heart_hz * t
                      + (frequency_modulation_depth_hz / respiratory_hz) * (1.0 - std::cos(2.0 * PI_CONST * respiratory_hz * t));
        double value = std::sin(phase);
        buffer.samples[i] = PPGWindowSample{0, 0, 0.0, value};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);
    return check_result(estimates.frequency_rpm, expected_rpm, "frequencia");
}

// Nível do canal bruto (linha de base) oscilando na FR, batimentos constantes.
bool check_baseline_modulation() {
    double expected_rpm = 15.0;
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = expected_rpm / 60.0;
    double baseline_center = 50000.0;
    double baseline_depth = 5000.0;

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double ir_filtered = std::sin(2.0 * PI_CONST * heart_hz * t);
        uint32_t ir_raw = static_cast<uint32_t>(baseline_center + baseline_depth * std::sin(2.0 * PI_CONST * respiratory_hz * t));
        buffer.samples[i] = PPGWindowSample{0, ir_raw, 0.0, ir_filtered};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);
    return check_result(estimates.baseline_rpm, expected_rpm, "linha de base");
}

// Espaçamento irregular entre batimentos (jitter aleatório) somado à modulação
// sistemática de frequência; amplitude e linha de base moduladas no instante
// real (deslocado pelo jitter) de cada batimento.
bool check_irregular_spacing() {
    double expected_rpm = 15.0;
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = expected_rpm / 60.0;
    double frequency_modulation_depth_hz = 0.15;
    double amplitude_modulation_depth = 0.3;
    double baseline_center = 50000.0;
    double baseline_depth = 5000.0;
    double jitter_fraction = 0.05; // +-5% de variação aleatória no intervalo entre batimentos
    constexpr double pulse_sigma_s = 0.05;

    // Semente fixa: jitter pseudo-aleatório, mas reprodutível.
    std::vector<double> beat_times;
    uint32_t seed = 12345u;
    auto next_unit_random = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<double>((seed >> 16) & 0x7fffu) / 32768.0; // [0,1)
    };

    for (double t = 0.0; t < 60.0;) {
        beat_times.push_back(t);
        double instantaneous_hz = heart_hz + frequency_modulation_depth_hz * std::sin(2.0 * PI_CONST * respiratory_hz * t);
        double nominal_period_s = 1.0 / instantaneous_hz;
        double jitter_s = (next_unit_random() * 2.0 - 1.0) * jitter_fraction * nominal_period_s;
        t += nominal_period_s + jitter_s;
    }

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double sample_t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;

        double ir_filtered = 0.0;
        for (double beat_t : beat_times) {
            double dx = sample_t - beat_t;
            if (std::abs(dx) > 4.0 * pulse_sigma_s) {
                continue;
            }
            double envelope = 1.0 + amplitude_modulation_depth * std::sin(2.0 * PI_CONST * respiratory_hz * beat_t);
            ir_filtered += envelope * std::exp(-(dx * dx) / (2.0 * pulse_sigma_s * pulse_sigma_s));
        }

        double ir_raw_value = baseline_center + baseline_depth * std::sin(2.0 * PI_CONST * respiratory_hz * sample_t);
        buffer.samples[i] = PPGWindowSample{0, static_cast<uint32_t>(ir_raw_value), 0.0, ir_filtered};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);
    bool ok = check_result(estimates.amplitude_rpm, expected_rpm, "amplitude, espacamento irregular");
    ok = check_result(estimates.frequency_rpm, expected_rpm, "frequencia, espacamento irregular") && ok;
    ok = check_result(estimates.baseline_rpm, expected_rpm, "linha de base, espacamento irregular") && ok;
    return ok;
}

// Modulação de AMPLITUDE a ~36 rpm, fora da faixa de busca [6,30] rpm. Não é
// pass/fail: só relata o que cada modulação retorna, não reprova a suíte.
bool report_out_of_range_frequency() {
    double true_rpm = 36.0; // 0.6 Hz, acima de PPG_RR_MAX_RPM
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = true_rpm / 60.0;
    double modulation_depth = 0.3;

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double envelope = 1.0 + modulation_depth * std::sin(2.0 * PI_CONST * respiratory_hz * t);
        double value = envelope * std::sin(2.0 * PI_CONST * heart_hz * t);
        buffer.samples[i] = PPGWindowSample{0, 0, 0.0, value};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);

    auto report = [](const char* label, const std::optional<double>& value) {
        if (value) {
            std::cout << "RELATO (" << label << ", FR real = 36 rpm, fora da faixa de busca): retornou "
                      << *value << " rpm -- esperado nullopt.\n";
        } else {
            std::cout << "RELATO (" << label << ", FR real = 36 rpm, fora da faixa de busca): nullopt, como esperado.\n";
        }
    };
    report("amplitude", estimates.amplitude_rpm);
    report("frequencia", estimates.frequency_rpm);
    report("linha de base", estimates.baseline_rpm);

    return true; // só relato — não reprova a suíte
}

// Sinal constante: após remover a média, todo bin do espectro fica com
// magnitude zero. Esperado: nullopt, não 0 rpm.
bool check_constant_signal_returns_absent() {
    size_t n = static_cast<size_t>(60.0 * PPG_RESPIRATORY_RESAMPLE_RATE_HZ);
    PPGRespiratorySignal constant_signal{std::vector<double>(n, 42.0), PPG_RESPIRATORY_RESAMPLE_PERIOD_S};

    auto result = measure_respiratory_frequency_rpm(constant_signal);
    if (result) {
        std::cerr << "FALHOU (sinal constante): esperava nullopt, obteve " << *result << " rpm\n";
        return false;
    }

    std::cout << "OK (sinal constante): nullopt, como esperado\n";
    return true;
}

// Smart Fusion, caso de concordância: as três modulações na mesma FR, sem jitter.
bool check_fusion_agreement() {
    double expected_rpm = 15.0;
    double heart_hz = 75.0 / 60.0;
    double respiratory_hz = expected_rpm / 60.0;
    double frequency_modulation_depth_hz = 0.15;
    double amplitude_modulation_depth = 0.3;
    double baseline_center = 50000.0;
    double baseline_depth = 5000.0;
    constexpr double pulse_sigma_s = 0.05;

    std::vector<double> beat_times;
    for (double t = 0.0; t < 60.0;) {
        beat_times.push_back(t);
        double instantaneous_hz = heart_hz + frequency_modulation_depth_hz * std::sin(2.0 * PI_CONST * respiratory_hz * t);
        t += 1.0 / instantaneous_hz;
    }

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double sample_t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;

        double ir_filtered = 0.0;
        for (double beat_t : beat_times) {
            double dx = sample_t - beat_t;
            if (std::abs(dx) > 4.0 * pulse_sigma_s) {
                continue;
            }
            double envelope = 1.0 + amplitude_modulation_depth * std::sin(2.0 * PI_CONST * respiratory_hz * beat_t);
            ir_filtered += envelope * std::exp(-(dx * dx) / (2.0 * pulse_sigma_s * pulse_sigma_s));
        }

        double ir_raw_value = baseline_center + baseline_depth * std::sin(2.0 * PI_CONST * respiratory_hz * sample_t);
        buffer.samples[i] = PPGWindowSample{0, static_cast<uint32_t>(ir_raw_value), 0.0, ir_filtered};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);
    return check_result(estimates.fused_rpm, expected_rpm, "fusao, tres concordam");
}

// Smart Fusion, caso de discordância: amplitude e frequência a 15 rpm, linha de
// base a 28 rpm (dentro da faixa de busca, mas violando o limite de discordância).
bool check_fusion_disagreement() {
    double heart_hz = 75.0 / 60.0;
    double agree_rpm = 15.0;
    double diverge_rpm = 28.0;
    double agree_hz = agree_rpm / 60.0;
    double diverge_hz = diverge_rpm / 60.0;
    double frequency_modulation_depth_hz = 0.15;
    double amplitude_modulation_depth = 0.3;
    double baseline_center = 50000.0;
    double baseline_depth = 5000.0;
    constexpr double pulse_sigma_s = 0.05;

    std::vector<double> beat_times;
    for (double t = 0.0; t < 60.0;) {
        beat_times.push_back(t);
        double instantaneous_hz = heart_hz + frequency_modulation_depth_hz * std::sin(2.0 * PI_CONST * agree_hz * t);
        t += 1.0 / instantaneous_hz;
    }

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double sample_t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;

        double ir_filtered = 0.0;
        for (double beat_t : beat_times) {
            double dx = sample_t - beat_t;
            if (std::abs(dx) > 4.0 * pulse_sigma_s) {
                continue;
            }
            double envelope = 1.0 + amplitude_modulation_depth * std::sin(2.0 * PI_CONST * agree_hz * beat_t);
            ir_filtered += envelope * std::exp(-(dx * dx) / (2.0 * pulse_sigma_s * pulse_sigma_s));
        }

        double ir_raw_value = baseline_center + baseline_depth * std::sin(2.0 * PI_CONST * diverge_hz * sample_t);
        buffer.samples[i] = PPGWindowSample{0, static_cast<uint32_t>(ir_raw_value), 0.0, ir_filtered};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);

    if (!estimates.amplitude_rpm || !estimates.frequency_rpm || !estimates.baseline_rpm) {
        std::cerr << "FALHOU (fusao, discordancia): esperava as tres modulacoes presentes pra exercitar o teste\n";
        return false;
    }
    if (estimates.fused_rpm) {
        std::cerr << "FALHOU (fusao, discordancia): valor fundido presente (" << *estimates.fused_rpm
                  << " rpm) -- esperava ausencia por discordancia (amplitude=" << *estimates.amplitude_rpm
                  << ", frequencia=" << *estimates.frequency_rpm << ", linha de base=" << *estimates.baseline_rpm
                  << ")\n";
        return false;
    }

    std::cout << "OK (fusao, discordancia): sem valor fundido, como esperado (amplitude=" << *estimates.amplitude_rpm
               << ", frequencia=" << *estimates.frequency_rpm << ", linha de base=" << *estimates.baseline_rpm
               << " rpm)\n";
    return true;
}

// Batimentos regulares (~75 bpm) com um intervalo anômalo de 3000 ms no meio da
// janela, acima de PPG_PULSE_PERIOD_MAX_MS.
bool check_pulse_period_artifact() {
    double heart_hz = 75.0 / 60.0;
    double nominal_period_s = 1.0 / heart_hz;
    constexpr double pulse_sigma_s = 0.05;
    double gap_start_s = 20.0;
    double gap_duration_s = 3.0; // 3000 ms > PPG_PULSE_PERIOD_MAX_MS

    std::vector<double> beat_times;
    for (double t = 0.0; t < 60.0; t += nominal_period_s) {
        if (t >= gap_start_s && t < gap_start_s + gap_duration_s) {
            continue;
        }
        beat_times.push_back(t);
    }

    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double sample_t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double ir_filtered = 0.0;
        for (double beat_t : beat_times) {
            double dx = sample_t - beat_t;
            if (std::abs(dx) > 4.0 * pulse_sigma_s) {
                continue;
            }
            ir_filtered += std::exp(-(dx * dx) / (2.0 * pulse_sigma_s * pulse_sigma_s));
        }
        buffer.samples[i] = PPGWindowSample{0, 50000, 0.0, ir_filtered};
    }
    buffer.count = buffer.buffer_capacity;

    auto estimates = calculate_respiratory_rate(last_window(buffer, buffer.buffer_capacity), PPG_SAMPLE_RATE_HZ);

    if (!estimates.pulse_period_artifact) {
        std::cerr << "FALHOU (artefato periodo de pulso): esperava pulse_period_artifact=true (gap de "
                  << gap_duration_s * 1000.0 << " ms)\n";
        return false;
    }
    if (estimates.amplitude_rpm || estimates.frequency_rpm || estimates.baseline_rpm || estimates.fused_rpm) {
        std::cerr << "FALHOU (artefato periodo de pulso): esperava as tres modulacoes e o valor fundido ausentes\n";
        return false;
    }

    std::cout << "OK (artefato periodo de pulso): detectado corretamente (gap de " << gap_duration_s * 1000.0
               << " ms)\n";
    return true;
}

} // namespace

int main() {
    bool ok_amplitude = check_amplitude_modulation();
    bool ok_frequency = check_frequency_modulation();
    bool ok_baseline = check_baseline_modulation();
    bool ok_irregular = check_irregular_spacing();
    bool ok_out_of_range_report = report_out_of_range_frequency();
    bool ok_constant_signal = check_constant_signal_returns_absent();
    bool ok_fusion_agreement = check_fusion_agreement();
    bool ok_fusion_disagreement = check_fusion_disagreement();
    bool ok_pulse_period_artifact = check_pulse_period_artifact();

    if (ok_amplitude && ok_frequency && ok_baseline && ok_irregular && ok_out_of_range_report
        && ok_constant_signal && ok_fusion_agreement && ok_fusion_disagreement && ok_pulse_period_artifact) {
        std::cout << "todos os testes de FR passaram\n";
        return 0;
    }

    std::cerr << "algum teste de FR falhou\n";
    return 1;
}
