#ifndef PPG_QUALITY_HPP
#define PPG_QUALITY_HPP

#include <vector>
#include "PPGBuffer.hpp"
#include "PPGPeaks.hpp"

// Limiares do Índice de Perfusão (ver README, seção 5, "Qualidade"); fundamentação
// do teto: Coutrot et al. (2021), 10.1016/j.accpm.2021.100964.
constexpr double PPG_QUALITY_MIN_PERFUSION_INDEX = 0.004; // 0.4%
constexpr double PPG_QUALITY_MAX_PERFUSION_INDEX = 0.15; // 15%

struct PPGPerfusionIndex {
    double red;
    double ir;
};

// PI = amplitude pico-a-pico do sinal filtrado (AC) / média do sinal bruto (DC).
PPGPerfusionIndex calculate_perfusion_index(PPGWindowView window);

struct PPGQualityResult {
    bool usable_for_hr;
    bool usable_for_spo2;
    bool usable_for_rr;
};

// window_30s alimenta usable_for_hr/usable_for_spo2; window_60s, usable_for_rr.
// raw_intensity (ver README, seção 4) desativa o critério de PI do IR para
// usable_for_hr e usable_for_rr; não afeta o canal vermelho de usable_for_spo2.
PPGQualityResult assess_window_quality(PPGWindowView window_30s, PPGWindowView window_60s, bool raw_intensity);

// Faixa de período de pulso plausível, conforme Karlen et al. (2013) — ver README.
constexpr double PPG_PULSE_PERIOD_MIN_MS = 230.0;
constexpr double PPG_PULSE_PERIOD_MAX_MS = 2400.0;

// true se algum intervalo entre picos consecutivos (já detectados por detect_peaks)
// cair fora de [PPG_PULSE_PERIOD_MIN_MS, PPG_PULSE_PERIOD_MAX_MS]. Usada apenas
// pela FR; recebe os picos já detectados por quem chama.
bool has_pulse_period_artifact(const std::vector<PPGPeak>& peaks, int sample_rate_hz);

#endif
