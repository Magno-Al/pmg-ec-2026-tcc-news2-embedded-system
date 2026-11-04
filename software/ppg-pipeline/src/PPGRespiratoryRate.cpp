#include <algorithm>
#include <cmath>
#include "PPGQuality.hpp"
#include "PPGRespiratoryRate.hpp"

namespace {

// Grade uniforme de PPG_RESPIRATORY_RESAMPLE_RATE_HZ cobrindo [t_start, t_end]
// (inclusive, até a tolerância de ponto flutuante).
std::vector<double> build_resample_grid(double t_start, double t_end) {
    std::vector<double> grid;
    for (double t = t_start; t <= t_end + 1e-9; t += PPG_RESPIRATORY_RESAMPLE_PERIOD_S) {
        grid.push_back(t);
    }
    return grid;
}

// Fora de [xs.front(), xs.back()], prende no valor da borda mais próxima.
double linear_interpolate(const std::vector<double>& xs, const std::vector<double>& ys, double x) {
    if (x <= xs.front()) {
        return ys.front();
    }
    if (x >= xs.back()) {
        return ys.back();
    }

    size_t upper = 1;
    while (xs[upper] < x) {
        upper++;
    }
    size_t lower = upper - 1;

    double fraction = (x - xs[lower]) / (xs[upper] - xs[lower]);
    return ys[lower] + fraction * (ys[upper] - ys[lower]);
}

// Ver extract_frequency_modulation em PPGRespiratoryRate.hpp.
double berger_instantaneous_rate(const std::vector<double>& beat_times, double t, double half_width_s) {
    double window_start = t - half_width_s;
    double window_end = t + half_width_s;
    double contribution_sum = 0.0;

    for (size_t i = 0; i + 1 < beat_times.size(); i++) {
        double interval_start = beat_times[i];
        double interval_end = beat_times[i + 1];
        double interval_duration = interval_end - interval_start;

        double overlap_start = std::max(interval_start, window_start);
        double overlap_end = std::min(interval_end, window_end);
        double overlap = overlap_end - overlap_start;

        if (overlap > 0.0) {
            contribution_sum += overlap / interval_duration;
        }
    }

    return contribution_sum / (2.0 * half_width_s);
}

// Pressupõe as três estimativas já presentes (ver calculate_respiratory_rate).
std::optional<double> fuse_respiratory_estimates(double amplitude_rpm, double frequency_rpm, double baseline_rpm) {
    double mean_rpm = (amplitude_rpm + frequency_rpm + baseline_rpm) / 3.0;

    double variance = ((amplitude_rpm - mean_rpm) * (amplitude_rpm - mean_rpm)
                      + (frequency_rpm - mean_rpm) * (frequency_rpm - mean_rpm)
                      + (baseline_rpm - mean_rpm) * (baseline_rpm - mean_rpm)) / 3.0;
    double std_dev_rpm = std::sqrt(variance);

    if (std_dev_rpm > PPG_RR_FUSION_MAX_DISAGREEMENT_RPM) {
        return std::nullopt;
    }

    return mean_rpm;
}

} // namespace

PPGRespiratorySignal extract_amplitude_modulation(const std::vector<PPGPeak>& peaks, PPGWindowView window,
                                                    int sample_rate_hz) {
    std::vector<double> peak_times;
    std::vector<double> peak_minus_foot;
    peak_times.reserve(peaks.size() - 1);
    peak_minus_foot.reserve(peaks.size() - 1);

    for (size_t i = 1; i < peaks.size(); i++) {
        double foot = window[peaks[i - 1].position].ir_filtered;
        for (size_t p = peaks[i - 1].position; p <= peaks[i].position; p++) {
            foot = std::min(foot, window[p].ir_filtered);
        }

        peak_times.push_back(static_cast<double>(peaks[i].position) / sample_rate_hz);
        peak_minus_foot.push_back(peaks[i].amplitude - foot);
    }

    double t_start = static_cast<double>(peaks.front().position) / sample_rate_hz;
    double t_end = static_cast<double>(peaks.back().position) / sample_rate_hz;
    std::vector<double> grid = build_resample_grid(t_start, t_end);

    std::vector<double> resampled;
    resampled.reserve(grid.size());
    for (double t : grid) {
        resampled.push_back(linear_interpolate(peak_times, peak_minus_foot, t));
    }

    return PPGRespiratorySignal{resampled, PPG_RESPIRATORY_RESAMPLE_PERIOD_S};
}

PPGRespiratorySignal extract_frequency_modulation(const std::vector<PPGPeak>& peaks, int sample_rate_hz) {
    std::vector<double> beat_times;
    beat_times.reserve(peaks.size());
    for (const auto& peak : peaks) {
        beat_times.push_back(static_cast<double>(peak.position) / sample_rate_hz);
    }

    std::vector<double> grid = build_resample_grid(beat_times.front(), beat_times.back());

    std::vector<double> resampled;
    resampled.reserve(grid.size());
    for (double t : grid) {
        resampled.push_back(berger_instantaneous_rate(beat_times, t, PPG_RESPIRATORY_RESAMPLE_PERIOD_S));
    }

    return PPGRespiratorySignal{resampled, PPG_RESPIRATORY_RESAMPLE_PERIOD_S};
}

PPGRespiratorySignal extract_baseline_modulation(const std::vector<PPGPeak>& peaks, PPGWindowView window,
                                                   int sample_rate_hz) {
    std::vector<double> peak_times;
    std::vector<double> baseline_levels;
    peak_times.reserve(peaks.size());
    baseline_levels.reserve(peaks.size());

    for (const auto& peak : peaks) {
        peak_times.push_back(static_cast<double>(peak.position) / sample_rate_hz);
        baseline_levels.push_back(static_cast<double>(window[peak.position].ir_raw));
    }

    std::vector<double> grid = build_resample_grid(peak_times.front(), peak_times.back());

    std::vector<double> resampled;
    resampled.reserve(grid.size());
    for (double t : grid) {
        resampled.push_back(linear_interpolate(peak_times, baseline_levels, t));
    }

    return PPGRespiratorySignal{resampled, PPG_RESPIRATORY_RESAMPLE_PERIOD_S};
}

PPGRespiratoryRateEstimates calculate_respiratory_rate(PPGWindowView window, int sample_rate_hz) {
    std::vector<PPGPeak> peaks = detect_peaks(window, PPGChannel::Ir, sample_rate_hz).peaks;

    PPGRespiratoryRateEstimates result;
    if (peaks.size() < PPG_RR_MIN_PEAKS) {
        return result;
    }

    if (has_pulse_period_artifact(peaks, sample_rate_hz)) {
        result.pulse_period_artifact = true;
        return result;
    }

    result.amplitude_rpm = measure_respiratory_frequency_rpm(extract_amplitude_modulation(peaks, window, sample_rate_hz));
    result.frequency_rpm = measure_respiratory_frequency_rpm(extract_frequency_modulation(peaks, sample_rate_hz));
    result.baseline_rpm = measure_respiratory_frequency_rpm(extract_baseline_modulation(peaks, window, sample_rate_hz));

    if (result.amplitude_rpm && result.frequency_rpm && result.baseline_rpm) {
        result.fused_rpm = fuse_respiratory_estimates(*result.amplitude_rpm, *result.frequency_rpm, *result.baseline_rpm);
    }

    return result;
}
