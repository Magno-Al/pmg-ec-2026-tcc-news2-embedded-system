#include <cmath>
#include "PPGRespiratorySignal.hpp"

namespace {

constexpr double PI_CONST = 3.14159265358979323846;

double mean_of(const std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum / values.size();
}

// w[n] = 0.54 - 0.46*cos(2*pi*n/(N-1)).
std::vector<double> apply_hamming_window(const std::vector<double>& values) {
    size_t n = values.size();
    std::vector<double> windowed(n);

    for (size_t i = 0; i < n; i++) {
        double w = 0.54 - 0.46 * std::cos(2.0 * PI_CONST * static_cast<double>(i) / static_cast<double>(n - 1));
        windowed[i] = values[i] * w;
    }

    return windowed;
}

struct SpectralBin {
    double frequency_hz;
    double magnitude;
};

// DFT direta; só os bins de frequência positiva (k = 1..n/2), sinal de entrada real.
std::vector<SpectralBin> compute_spectrum(const std::vector<double>& windowed_values, double sample_period_s) {
    size_t n = windowed_values.size();
    std::vector<SpectralBin> spectrum;
    spectrum.reserve(n / 2);

    for (size_t k = 1; k <= n / 2; k++) {
        double real = 0.0;
        double imag = 0.0;

        for (size_t t = 0; t < n; t++) {
            double angle = 2.0 * PI_CONST * static_cast<double>(k) * static_cast<double>(t) / static_cast<double>(n);
            real += windowed_values[t] * std::cos(angle);
            imag -= windowed_values[t] * std::sin(angle);
        }

        double frequency_hz = static_cast<double>(k) / (static_cast<double>(n) * sample_period_s);
        spectrum.push_back(SpectralBin{frequency_hz, std::sqrt(real * real + imag * imag)});
    }

    return spectrum;
}

} // namespace

std::optional<double> measure_respiratory_frequency_rpm(const PPGRespiratorySignal& signal) {
    double mean_value = mean_of(signal.values);
    std::vector<double> centered;
    centered.reserve(signal.values.size());
    for (double value : signal.values) {
        centered.push_back(value - mean_value);
    }

    std::vector<double> windowed = apply_hamming_window(centered);

    std::vector<SpectralBin> spectrum = compute_spectrum(windowed, signal.sampling_period_s);

    SpectralBin dominant{0.0, 0.0};
    size_t bins_in_range = 0;

    for (const auto& bin : spectrum) {
        if (bin.frequency_hz < PPG_RR_MIN_FREQUENCY_HZ || bin.frequency_hz > PPG_RR_MAX_FREQUENCY_HZ) {
            continue;
        }

        bins_in_range++;
        if (bin.magnitude > dominant.magnitude) {
            dominant = bin;
        }
    }

    if (bins_in_range == 0 || dominant.magnitude <= 0.0) {
        return std::nullopt;
    }

    return dominant.frequency_hz * 60.0;
}
