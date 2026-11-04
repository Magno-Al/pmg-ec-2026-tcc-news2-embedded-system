#ifndef PPG_HEART_RATE_HPP
#define PPG_HEART_RATE_HPP

#include <cstddef>
#include <optional>
#include "PPGBuffer.hpp"

// Faixa fisiológica plausível (ver README, seção 5).
constexpr double PPG_HEART_RATE_MIN_BPM = 30.0;
constexpr double PPG_HEART_RATE_MAX_BPM = 240.0;
constexpr size_t PPG_HEART_RATE_MIN_PEAKS = 3; // mínimo de picos para uma mediana confiável

struct PPGHeartRateResult {
    std::optional<double> bpm;
    size_t peak_count = 0; // picos detectados na janela — informativo mesmo quando bpm é nullopt
};

// Detecta os picos de pulso no canal infravermelho (ver detect_peaks em
// PPGPeaks.hpp), converte os intervalos entre picos consecutivos em bpm
// instantâneo e agrega pela mediana. bpm vem nullopt se não houver picos
// suficientes ou se o resultado sair da faixa fisiológica; peak_count fica
// preenchido mesmo nesse caso.
PPGHeartRateResult calculate_heart_rate(PPGWindowView window, int sample_rate_hz);

#endif
