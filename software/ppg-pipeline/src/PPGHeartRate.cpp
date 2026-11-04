#include <algorithm>
#include <vector>
#include "PPGHeartRate.hpp"
#include "PPGPeaks.hpp"

namespace {

double median_of(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    size_t n = values.size();
    if (n % 2 == 1) {
        return values[n / 2];
    }
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

} // namespace

PPGHeartRateResult calculate_heart_rate(PPGWindowView window, int sample_rate_hz) {
    std::vector<PPGPeak> peaks = detect_peaks(window, PPGChannel::Ir, sample_rate_hz).peaks;

    PPGHeartRateResult result;
    result.peak_count = peaks.size();

    if (peaks.size() < PPG_HEART_RATE_MIN_PEAKS) {
        return result;
    }

    std::vector<double> instantaneous_bpm;
    instantaneous_bpm.reserve(peaks.size() - 1);
    for (size_t i = 1; i < peaks.size(); i++) {
        double interval_s = static_cast<double>(peaks[i].position - peaks[i - 1].position) / sample_rate_hz;
        instantaneous_bpm.push_back(60.0 / interval_s);
    }

    double heart_rate_bpm = median_of(instantaneous_bpm);
    if (heart_rate_bpm < PPG_HEART_RATE_MIN_BPM || heart_rate_bpm > PPG_HEART_RATE_MAX_BPM) {
        return result;
    }

    result.bpm = heart_rate_bpm;
    return result;
}
