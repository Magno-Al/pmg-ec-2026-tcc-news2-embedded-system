#include <cmath>
#include <iostream>
#include "PPG.hpp"
#include "PPGBuffer.hpp"
#include "PPGHeartRate.hpp"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double TOLERANCE_BPM = 2.0;

// Onda periódica sintética na taxa de amostragem do projeto, simulando um canal
// infravermelho já filtrado (zero-mean) com FC conhecida.
PPGBuffer make_synthetic_window(double bpm) {
    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    double frequency_hz = bpm / 60.0;

    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double value = std::sin(2.0 * PI * frequency_hz * t);
        buffer.samples[i] = PPGWindowSample{0, 0, 0.0, value};
    }
    buffer.count = buffer.buffer_capacity;

    return buffer;
}

bool check_heart_rate(double expected_bpm) {
    PPGBuffer buffer = make_synthetic_window(expected_bpm);
    size_t hr_spo2_window_samples = seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ);
    auto result = calculate_heart_rate(last_window(buffer, hr_spo2_window_samples), PPG_SAMPLE_RATE_HZ);

    if (!result.bpm) {
        std::cerr << "FALHOU: nao foi possivel calcular FC para sinal sintetico de "
                  << expected_bpm << " bpm (picos detectados: " << result.peak_count << ")\n";
        return false;
    }

    double error = std::abs(*result.bpm - expected_bpm);
    if (error > TOLERANCE_BPM) {
        std::cerr << "FALHOU: FC calculada " << *result.bpm << " bpm, esperado " << expected_bpm
                  << " bpm (erro " << error << " > " << TOLERANCE_BPM << ")\n";
        return false;
    }

    std::cout << "OK: FC calculada " << *result.bpm << " bpm (esperado " << expected_bpm
               << " bpm, " << result.peak_count << " picos)\n";
    return true;
}

} // namespace

int main() {
    bool ok_60_bpm = check_heart_rate(60.0);
    bool ok_90_bpm = check_heart_rate(90.0);

    if (ok_60_bpm && ok_90_bpm) {
        std::cout << "todos os testes de FC passaram\n";
        return 0;
    }

    std::cerr << "algum teste de FC falhou\n";
    return 1;
}
