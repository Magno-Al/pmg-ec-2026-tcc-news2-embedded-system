#include <cmath>
#include <iostream>
#include "PPG.hpp"
#include "PPGBuffer.hpp"
#include "PPGSpO2.hpp"

namespace {

constexpr double PI_CONST = 3.14159265358979323846;
constexpr double TOLERANCE_SPO2 = 0.5;

// Janela sintética com DC/AC independentes por canal. red_ac_amplitude/ir_ac_amplitude
// são a amplitude do seno (não o pico-a-pico) -- PI de um canal = (2*amplitude)/dc.
PPGBuffer make_synthetic_window(double red_dc, double red_ac_amplitude, double ir_dc, double ir_ac_amplitude) {
    PPGBuffer buffer = make_ppg_buffer(PPG_SAMPLE_RATE_HZ);
    uint32_t red_raw_dc = static_cast<uint32_t>(red_dc);
    uint32_t ir_raw_dc = static_cast<uint32_t>(ir_dc);

    for (size_t i = 0; i < buffer.buffer_capacity; i++) {
        double t = static_cast<double>(i) / PPG_SAMPLE_RATE_HZ;
        double red_oscillation = red_ac_amplitude * std::sin(2.0 * PI_CONST * 1.2 * t);
        double ir_oscillation = ir_ac_amplitude * std::sin(2.0 * PI_CONST * 1.2 * t);
        buffer.samples[i] = PPGWindowSample{red_raw_dc, ir_raw_dc, red_oscillation, ir_oscillation};
    }
    buffer.count = buffer.buffer_capacity;

    return buffer;
}

bool check_known_r_gives_expected_spo2() {
    // ir: DC=50000, AC pico-a-pico=1000 -> PI_ir=2%
    // red: DC=50000, AC pico-a-pico=500  -> PI_red=1%
    // R = PI_red/PI_ir = 0.5 -> SpO2 = 110 - 25*0.5 = 97.5%
    PPGBuffer buffer = make_synthetic_window(50000.0, 250.0, 50000.0, 500.0);
    auto result = calculate_spo2(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)));

    if (!result) {
        std::cerr << "FALHOU: nao foi possivel calcular SpO2 para R conhecido\n";
        return false;
    }

    double expected = 97.5;
    double error = std::abs(*result - expected);
    if (error > TOLERANCE_SPO2) {
        std::cerr << "FALHOU: SpO2 calculado " << *result << "%, esperado " << expected
                  << "% (erro " << error << " > " << TOLERANCE_SPO2 << ")\n";
        return false;
    }

    std::cout << "OK: SpO2 calculado " << *result << "% (esperado " << expected << "%)\n";
    return true;
}

bool check_implausible_r_returns_nullopt() {
    // ir: DC=50000, AC pico-a-pico=1000 -> PI_ir=2%
    // red: DC=50000, AC pico-a-pico=100 -> PI_red=0.2%
    // R = 0.002/0.02 = 0.1 -> SpO2 = 110 - 2.5 = 107.5% (acima de 100%, implausivel)
    PPGBuffer buffer = make_synthetic_window(50000.0, 50.0, 50000.0, 500.0);
    auto result = calculate_spo2(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)));

    if (result) {
        std::cerr << "FALHOU: SpO2 implausivel (" << *result << "%) deveria ser nullopt\n";
        return false;
    }

    std::cout << "OK: R que resultaria em SpO2 > 100% foi corretamente descartado (nullopt)\n";
    return true;
}

} // namespace

int main() {
    bool ok_known = check_known_r_gives_expected_spo2();
    bool ok_implausible = check_implausible_r_returns_nullopt();

    if (ok_known && ok_implausible) {
        std::cout << "todos os testes de SpO2 passaram\n";
        return 0;
    }

    std::cerr << "algum teste de SpO2 falhou\n";
    return 1;
}
