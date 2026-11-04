#include "PPGPeaks.hpp"
#include "PPGMsptdBeatDetector.hpp"

namespace {

double channel_value(const PPGWindowSample& sample, PPGChannel channel) {
    return channel == PPGChannel::Red ? sample.red_filtered : sample.ir_filtered;
}

} // namespace

PPGPeakDetectionResult detect_peaks(PPGWindowView window, PPGChannel channel, int sample_rate_hz) {
    std::vector<double> values;
    values.reserve(window.size());
    for (const auto& sample : window) {
        values.push_back(channel_value(sample, channel));
    }

    PPGMsptdResult msptd_result = msptd_beat_detector(values, sample_rate_hz);

    PPGPeakDetectionResult result;
    result.peaks.reserve(msptd_result.peaks.size());
    for (size_t position : msptd_result.peaks) {
        result.peaks.push_back(PPGPeak{position, values[position]});
    }
    result.onsets = std::move(msptd_result.onsets);

    return result;
}
