// Teste isolado do MSPTD (ver PPGMsptdBeatDetector.hpp/.cpp): sinal sintético
// com pulsos em posições conhecidas, sem passar pelo resto do pipeline.
#include <cmath>
#include <iostream>
#include <vector>
#include "PPGMsptdBeatDetector.hpp"

namespace {

constexpr int SAMPLE_RATE_HZ = 86;

// Pulsos gaussianos não sobrepostos, com centros exatamente nas posições dadas.
std::vector<double> make_pulse_train(const std::vector<size_t>& pulse_centers, size_t total_samples, double sigma_samples) {
    std::vector<double> signal(total_samples, 0.0);
    for (size_t i = 0; i < total_samples; i++) {
        double value = 0.0;
        for (size_t center : pulse_centers) {
            double dx = static_cast<double>(i) - static_cast<double>(center);
            value += std::exp(-(dx * dx) / (2.0 * sigma_samples * sigma_samples));
        }
        signal[i] = value;
    }
    return signal;
}

// 1000 amostras a 86 Hz: mais longo que a janela interna de 6s (516 amostras),
// exercitando o janelamento externo com sobreposição e a deduplicação entre
// janelas (três janelas internas, [0,516], [413,929] e [483,999]). Pulsos de
// borda ficam com margem generosa (>=65 amostras) das bordas de qualquer janela
// que os cubra, já que perto da borda o próprio MSPTD pode não marcar um pico.
bool check_known_pulse_positions() {
    std::vector<size_t> expected_peaks = {90, 176, 262, 348, 434, 520, 606, 692, 778, 864};
    size_t total_samples = 1000;
    double sigma_samples = 8.0;

    std::vector<double> signal = make_pulse_train(expected_peaks, total_samples, sigma_samples);

    PPGMsptdResult result = msptd_beat_detector(signal, SAMPLE_RATE_HZ);

    if (result.peaks.size() != expected_peaks.size()) {
        std::cerr << "FALHOU: " << result.peaks.size() << " picos detectados, esperado "
                  << expected_peaks.size() << '\n';
        for (size_t p : result.peaks) {
            std::cerr << "  pico em " << p << '\n';
        }
        return false;
    }

    for (size_t i = 0; i < expected_peaks.size(); i++) {
        if (result.peaks[i] != expected_peaks[i]) {
            std::cerr << "FALHOU: pico " << i << " detectado em " << result.peaks[i] << ", esperado em "
                      << expected_peaks[i] << '\n';
            return false;
        }
    }

    // Pelo menos 9 vales entre os 10 pulsos; pode haver mais perto das bordas.
    if (result.onsets.size() < expected_peaks.size() - 1) {
        std::cerr << "FALHOU: " << result.onsets.size() << " inicios de pulso detectados, esperado pelo menos "
                  << (expected_peaks.size() - 1) << '\n';
        return false;
    }

    std::cout << "OK: " << result.peaks.size() << " picos detectados nas posicoes exatas esperadas, "
              << result.onsets.size() << " inicios de pulso\n";
    return true;
}

// Sinal mais curto que uma janela interna (6s = 516 amostras a 86 Hz): usa o ramo
// de "uma janela só" do janelamento externo (ver msptd_beat_detector).
bool check_single_internal_window() {
    std::vector<size_t> expected_peaks = {60, 140, 220, 300};
    size_t total_samples = 380; // < 516, uma única janela interna
    double sigma_samples = 6.0;

    std::vector<double> signal = make_pulse_train(expected_peaks, total_samples, sigma_samples);
    PPGMsptdResult result = msptd_beat_detector(signal, SAMPLE_RATE_HZ);

    if (result.peaks.size() != expected_peaks.size()) {
        std::cerr << "FALHOU (janela unica): " << result.peaks.size() << " picos detectados, esperado "
                  << expected_peaks.size() << '\n';
        return false;
    }
    for (size_t i = 0; i < expected_peaks.size(); i++) {
        if (result.peaks[i] != expected_peaks[i]) {
            std::cerr << "FALHOU (janela unica): pico " << i << " detectado em " << result.peaks[i]
                      << ", esperado em " << expected_peaks[i] << '\n';
            return false;
        }
    }

    std::cout << "OK (janela unica): " << result.peaks.size() << " picos detectados nas posicoes exatas\n";
    return true;
}

} // namespace

int main() {
    bool ok_multi_window = check_known_pulse_positions();
    bool ok_single_window = check_single_internal_window();

    if (ok_multi_window && ok_single_window) {
        std::cout << "todos os testes de MSPTD passaram\n";
        return 0;
    }

    std::cerr << "algum teste de MSPTD falhou\n";
    return 1;
}
