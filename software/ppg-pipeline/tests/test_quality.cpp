#include <cmath>
#include <iostream>
#include "PPG.hpp"
#include "PPGBuffer.hpp"
#include "PPGQuality.hpp"

namespace {

constexpr double PI_CONST = 3.14159265358979323846;

// Janela sintética com DC/AC independentes por canal, pra poder simular red e ir
// com qualidade diferente. PI de um canal = (2 * ac_amplitude) / dc_level.
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

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FALHOU: " << message << '\n';
        return false;
    }
    std::cout << "OK: " << message << '\n';
    return true;
}

bool check_both_channels_in_range() {
    // red e ir com DC=50000, AC pico-a-pico=1000 -> PI=2% nos dois (dentro da faixa)
    PPGBuffer buffer = make_synthetic_window(50000.0, 500.0, 50000.0, 500.0);
    PPGQualityResult result = assess_window_quality(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)),
                                                      last_window(buffer, buffer.buffer_capacity), true);

    bool ok = expect(result.usable_for_hr, "ambos os canais na faixa -> usavel para FC");
    ok = expect(result.usable_for_spo2, "ambos os canais na faixa -> usavel para SpO2") && ok;
    ok = expect(result.usable_for_rr, "ir na faixa (60s) -> usavel para FR") && ok;
    return ok;
}

bool check_red_out_of_range_only() {
    // ir com PI=2% (na faixa); red com PI=0.02% (abaixo do minimo)
    PPGBuffer buffer = make_synthetic_window(50000.0, 5.0, 50000.0, 500.0);
    PPGQualityResult result = assess_window_quality(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)),
                                                      last_window(buffer, buffer.buffer_capacity), true);

    bool ok = expect(result.usable_for_hr, "ir na faixa, red ruim -> ainda usavel para FC");
    ok = expect(!result.usable_for_spo2, "ir na faixa, red ruim -> nao usavel para SpO2") && ok;
    ok = expect(result.usable_for_rr, "ir na faixa (60s), red ruim -> ainda usavel para FR") && ok;
    return ok;
}

bool check_ir_out_of_range() {
    // ir com PI=0.02% (abaixo do minimo); red na faixa (nao deveria importar)
    PPGBuffer buffer = make_synthetic_window(50000.0, 500.0, 50000.0, 5.0);
    PPGQualityResult result = assess_window_quality(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)),
                                                      last_window(buffer, buffer.buffer_capacity), true);

    bool ok = expect(!result.usable_for_hr, "ir ruim -> nao usavel para FC");
    ok = expect(!result.usable_for_spo2, "ir ruim -> nao usavel para SpO2 mesmo com red bom") && ok;
    ok = expect(!result.usable_for_rr, "ir ruim (60s) -> nao usavel para FR") && ok;
    return ok;
}

bool check_raw_intensity_false_bypasses_ir_pi() {
    // ir com PI=0.02% (fora da faixa); red com PI=0.02% (também fora, de proposito)
    PPGBuffer buffer = make_synthetic_window(50000.0, 5.0, 50000.0, 5.0);
    PPGQualityResult result = assess_window_quality(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)),
                                                      last_window(buffer, buffer.buffer_capacity), false);

    bool ok = expect(result.usable_for_hr, "raw_intensity=false, ir fora da faixa -> ainda usavel para FC");
    ok = expect(result.usable_for_rr, "raw_intensity=false, ir fora da faixa (60s) -> ainda usavel para FR") && ok;
    ok = expect(!result.usable_for_spo2, "raw_intensity=false, red fora da faixa -> SpO2 continua nao usavel") && ok;
    return ok;
}

bool check_raw_intensity_false_no_effect_when_already_in_range() {
    PPGBuffer buffer = make_synthetic_window(50000.0, 500.0, 50000.0, 500.0);
    PPGQualityResult result = assess_window_quality(last_window(buffer, seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, PPG_SAMPLE_RATE_HZ)),
                                                      last_window(buffer, buffer.buffer_capacity), false);

    bool ok = expect(result.usable_for_hr, "raw_intensity=false, ir ja na faixa -> continua usavel para FC");
    ok = expect(result.usable_for_spo2, "raw_intensity=false, ambos ja na faixa -> continua usavel para SpO2") && ok;
    ok = expect(result.usable_for_rr, "raw_intensity=false, ir ja na faixa (60s) -> continua usavel para FR") && ok;
    return ok;
}

} // namespace

int main() {
    bool ok_both = check_both_channels_in_range();
    bool ok_red_bad = check_red_out_of_range_only();
    bool ok_ir_bad = check_ir_out_of_range();
    bool ok_raw_intensity_bypass = check_raw_intensity_false_bypasses_ir_pi();
    bool ok_raw_intensity_no_effect = check_raw_intensity_false_no_effect_when_already_in_range();

    if (ok_both && ok_red_bad && ok_ir_bad && ok_raw_intensity_bypass && ok_raw_intensity_no_effect) {
        std::cout << "todos os testes de qualidade (PI por parametro) passaram\n";
        return 0;
    }

    std::cerr << "algum teste de qualidade (PI por parametro) falhou\n";
    return 1;
}
