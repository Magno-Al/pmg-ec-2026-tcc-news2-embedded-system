#ifndef PPG_PEAKS_HPP
#define PPG_PEAKS_HPP

#include <cstddef>
#include <vector>
#include "PPGBuffer.hpp"

// Posição (índice da amostra na janela) e amplitude do sinal filtrado nesse ponto.
struct PPGPeak {
    size_t position;
    double amplitude;
};

struct PPGPeakDetectionResult {
    std::vector<PPGPeak> peaks;
    std::vector<size_t> onsets;
};

// Detecta picos e inícios de pulso do canal escolhido no sinal filtrado, pelo
// MSPTD (ver README, seção 5, "Batimentos", e PPGMsptdBeatDetector.hpp/.cpp).
PPGPeakDetectionResult detect_peaks(PPGWindowView window, PPGChannel channel, int sample_rate_hz);

#endif
