#ifndef PPG_DEBUG_HPP
#define PPG_DEBUG_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>

// Abre um CSV para dump contínuo amostra a amostra (cria os diretórios do caminho se
// necessário) e já escreve o cabeçalho.
std::ofstream open_ppg_debug_dump(const std::string& path);

void write_ppg_debug_sample(std::ofstream& file, size_t index,
                             uint32_t red_raw, double red_filtered,
                             uint32_t ir_raw, double ir_filtered);

// Abre um CSV com o veredito (aceita/descartada) de cada janela, pra permitir
// visualizar depois em cima do gráfico da gravação inteira.
std::ofstream open_ppg_window_dump(const std::string& path);

void write_ppg_window_record(std::ofstream& file, int window_index,
                              size_t start_sample, size_t end_sample, bool accepted);

// Abre um CSV para validação: uma linha por janela com os parâmetros fisiológicos
// já calculados pelo pipeline de produção -- este módulo só observa e registra,
// não recalcula nada. Um valor vazio (nullopt) é escrito como campo vazio no CSV.
//
// Status de hr_status/spo2_status: "aceita" (usável e com valor), "aceita_sem_fc"
// / "aceita_sem_spo2" (usável, mas sem valor confiável), "descartada" (controle de
// qualidade reprovou o canal necessário).
//
// rr_status/rr_value são o valor FUNDIDO (Smart Fusion, ver PPGRespiratoryRate.hpp).
// Status: "aceita", "descartada" (falhou no controle de qualidade),
// "artefato_periodo_pulso" (ver has_pulse_period_artifact, PPGQuality.hpp),
// "estimativa_ausente" (alguma modulação sem pico espectral confiável),
// "discordancia" (desvio entre as três acima de PPG_RR_FUSION_MAX_DISAGREEMENT_RPM).
// rr_amplitude_*/rr_frequency_*/rr_baseline_* são as três modulações individuais,
// cada uma com seu próprio status/valor.
struct PPGParameterRecord {
    double start_time_s;
    double end_time_s;

    std::string hr_status;
    std::optional<double> hr_value;
    std::optional<size_t> hr_peak_count;

    std::string spo2_status;
    std::optional<double> spo2_value;

    std::string rr_status;
    std::optional<double> rr_value;

    std::string rr_amplitude_status;
    std::optional<double> rr_amplitude_value;

    std::string rr_frequency_status;
    std::optional<double> rr_frequency_value;

    std::string rr_baseline_status;
    std::optional<double> rr_baseline_value;

    // PI das janelas de 30s (red/ir) e 60s (ir); só observabilidade, sempre
    // presente (não depende da janela ter passado no controle de qualidade).
    double pi_red_30s;
    double pi_ir_30s;
    double pi_ir_60s;
};

std::ofstream open_ppg_parameters_export(const std::string& path);

void write_ppg_parameters_record(std::ofstream& file, const PPGParameterRecord& record);

#endif
