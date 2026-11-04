#include "PPGSpO2.hpp"
#include "PPGQuality.hpp"

std::optional<double> calculate_spo2(PPGWindowView window) {
    PPGPerfusionIndex pi = calculate_perfusion_index(window);

    if (pi.ir == 0.0) {
        return std::nullopt;
    }

    double r = pi.red / pi.ir;
    double spo2 = PPG_SPO2_CALIBRATION_A - PPG_SPO2_CALIBRATION_B * r;

    if (spo2 < PPG_SPO2_MIN_PLAUSIBLE || spo2 > PPG_SPO2_MAX_PLAUSIBLE) {
        return std::nullopt;
    }

    return spo2;
}
