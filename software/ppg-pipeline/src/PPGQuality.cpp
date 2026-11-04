#include <algorithm>
#include "PPGQuality.hpp"

namespace {

double peak_to_peak_filtered(PPGWindowView window, bool is_red) {
    double min_value = is_red ? window[0].red_filtered : window[0].ir_filtered;
    double max_value = min_value;

    for (const auto& sample : window) {
        double value = is_red ? sample.red_filtered : sample.ir_filtered;
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }

    return max_value - min_value;
}

double mean_raw(PPGWindowView window, bool is_red) {
    double sum = 0.0;
    for (const auto& sample : window) {
        sum += is_red ? sample.red_raw : sample.ir_raw;
    }
    return sum / window.size();
}

double safe_ratio(double numerator, double denominator) {
    return denominator != 0.0 ? numerator / denominator : 0.0;
}

bool perfusion_index_in_range(double pi) {
    return pi >= PPG_QUALITY_MIN_PERFUSION_INDEX && pi <= PPG_QUALITY_MAX_PERFUSION_INDEX;
}

} // namespace

PPGPerfusionIndex calculate_perfusion_index(PPGWindowView window) {
    double red_ac = peak_to_peak_filtered(window, true);
    double ir_ac = peak_to_peak_filtered(window, false);
    double red_dc = mean_raw(window, true);
    double ir_dc = mean_raw(window, false);

    return PPGPerfusionIndex{safe_ratio(red_ac, red_dc), safe_ratio(ir_ac, ir_dc)};
}

PPGQualityResult assess_window_quality(PPGWindowView window_30s, PPGWindowView window_60s, bool raw_intensity) {
    PPGPerfusionIndex pi_30s = calculate_perfusion_index(window_30s);
    PPGPerfusionIndex pi_60s = calculate_perfusion_index(window_60s);

    bool ir_30s_ok = !raw_intensity || perfusion_index_in_range(pi_30s.ir);
    bool ir_60s_ok = !raw_intensity || perfusion_index_in_range(pi_60s.ir);

    PPGQualityResult result;
    result.usable_for_hr = ir_30s_ok;
    result.usable_for_spo2 = result.usable_for_hr && perfusion_index_in_range(pi_30s.red);
    result.usable_for_rr = ir_60s_ok;
    return result;
}

bool has_pulse_period_artifact(const std::vector<PPGPeak>& peaks, int sample_rate_hz) {
    for (size_t i = 1; i < peaks.size(); i++) {
        double interval_ms =
            1000.0 * static_cast<double>(peaks[i].position - peaks[i - 1].position) / static_cast<double>(sample_rate_hz);
        if (interval_ms < PPG_PULSE_PERIOD_MIN_MS || interval_ms > PPG_PULSE_PERIOD_MAX_MS) {
            return true;
        }
    }
    return false;
}
