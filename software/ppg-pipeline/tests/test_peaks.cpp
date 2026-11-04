#include <cmath>
#include <iostream>
#include "PPG.hpp"
#include "PPGBuffer.hpp"
#include "PPGPeaks.hpp"

namespace {

constexpr double PI_CONST = 3.14159265358979323846;

// Onda periódica sintética no canal IR, com N picos conhecidos (um por período).
PPGBuffer make_synthetic_window(double frequency_hz) {
    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);

    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double value = std::sin(2.0 * PI_CONST * frequency_hz * t);
        buffer.samples[i] = PPGWindowSample{0, 0, 0.0, value};
    }
    buffer.count = buffer.buffer_capacity;

    return buffer;
}

bool check_known_peak_count() {
    // 1.5 Hz por PPG_WINDOW_SECONDS_HR_SPO2 (30s) -> 45 ciclos -> ~45 picos esperados.
    double frequency_hz = 1.5;
    PPGBuffer buffer = make_synthetic_window(frequency_hz);
    PPGWindowView window = last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ));

    auto peaks = detect_peaks(window, PPGChannel::Ir, PPG_SAMPLE_RATE_HZ).peaks;

    int expected_peaks = static_cast<int>(frequency_hz * PPG_WINDOW_SECONDS_HR_SPO2);
    int error = std::abs(static_cast<int>(peaks.size()) - expected_peaks);
    if (error > 2) {
        std::cerr << "FALHOU: " << peaks.size() << " picos detectados, esperado ~" << expected_peaks << '\n';
        return false;
    }

    // Cada pico deve estar perto do topo do seno (amplitude ~1.0) e espaçado ~1/frequency_hz segundos.
    for (size_t i = 0; i < peaks.size(); i++) {
        if (peaks[i].amplitude < 0.9) {
            std::cerr << "FALHOU: pico " << i << " com amplitude " << peaks[i].amplitude << ", esperado perto de 1.0\n";
            return false;
        }
        if (i > 0) {
            double spacing_s = static_cast<double>(peaks[i].position - peaks[i - 1].position) / PPG_SAMPLE_RATE_HZ;
            double expected_spacing_s = 1.0 / frequency_hz;
            if (std::abs(spacing_s - expected_spacing_s) > 0.05) {
                std::cerr << "FALHOU: espaçamento entre picos " << i - 1 << " e " << i << " foi " << spacing_s
                          << "s, esperado ~" << expected_spacing_s << "s\n";
                return false;
            }
        }
    }

    std::cout << "OK: " << peaks.size() << " picos detectados (esperado ~" << expected_peaks
              << "), posições e amplitudes plausíveis\n";
    return true;
}

} // namespace

int main() {
    if (check_known_peak_count()) {
        std::cout << "todos os testes de detecção de picos passaram\n";
        return 0;
    }

    std::cerr << "algum teste de detecção de picos falhou\n";
    return 1;
}
