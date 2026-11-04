#ifndef PPG_RESPIRATORY_RATE_HPP
#define PPG_RESPIRATORY_RATE_HPP

#include <cstddef>
#include <optional>
#include <vector>
#include "PPGBuffer.hpp"
#include "PPGPeaks.hpp"
#include "PPGRespiratorySignal.hpp"

constexpr size_t PPG_RR_MIN_PEAKS = 20;

// Taxa de reamostragem das três modulações respiratórias (ver README, seção 5,
// "FR"). A grade cobre o intervalo entre o primeiro e o último pico da janela.
constexpr double PPG_RESPIRATORY_RESAMPLE_RATE_HZ = 4.0;
constexpr double PPG_RESPIRATORY_RESAMPLE_PERIOD_S = 1.0 / PPG_RESPIRATORY_RESAMPLE_RATE_HZ;

// Amplitude = pico menos pé (mínimo do sinal entre o pico anterior e o atual);
// diferente de PPGPeak::amplitude, que não referencia um pé/vale. N picos geram
// N-1 alturas, interpoladas linearmente sobre a grade de reamostragem.
PPGRespiratorySignal extract_amplitude_modulation(const std::vector<PPGPeak>& peaks, PPGWindowView window,
                                                    int sample_rate_hz);

// Frequência cardíaca instantânea em cada ponto da grade, pelo algoritmo de
// Berger et al. (1986): cada intervalo entre batimentos contribui com a fração
// dele sobreposta a uma janela local de largura 2*PPG_RESPIRATORY_RESAMPLE_PERIOD_S
// centrada no ponto, dividida pela duração do intervalo; a soma dessas frações
// dividida pela largura da janela local é o valor reamostrado nesse ponto.
PPGRespiratorySignal extract_frequency_modulation(const std::vector<PPGPeak>& peaks, int sample_rate_hz);

// Linha de base = valor do canal bruto (ir_raw) na posição de cada pico,
// interpolado linearmente sobre a grade de reamostragem, igual à amplitude.
PPGRespiratorySignal extract_baseline_modulation(const std::vector<PPGPeak>& peaks, PPGWindowView window,
                                                   int sample_rate_hz);

// Limite de discordância (desvio padrão populacional, em rpm) do Smart Fusion
// (ver README, seção 5, "FR").
constexpr double PPG_RR_FUSION_MAX_DISAGREEMENT_RPM = 4.0;

// pulse_period_artifact=true quando a janela foi descartada por artefato de
// período de pulso (ver has_pulse_period_artifact em PPGQuality.hpp); nesse caso
// as três estimativas e fused_rpm vêm nullopt.
struct PPGRespiratoryRateEstimates {
    std::optional<double> amplitude_rpm;
    std::optional<double> frequency_rpm;
    std::optional<double> baseline_rpm;
    bool pulse_period_artifact = false;
    std::optional<double> fused_rpm;
};

// Ordem: detect_peaks -> has_pulse_period_artifact (aplicada antes de qualquer
// extração/reamostragem) -> as três extrações -> Smart Fusion, só se as três
// estimativas estiverem presentes. Ver README, seção 5, "FR".
PPGRespiratoryRateEstimates calculate_respiratory_rate(PPGWindowView window, int sample_rate_hz);

#endif
