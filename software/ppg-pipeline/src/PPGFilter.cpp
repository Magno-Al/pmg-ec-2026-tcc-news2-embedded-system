#include <cmath>
#include "PPGFilter.hpp"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double BUTTERWORTH_Q = 0.70710678118654752; // 1/sqrt(2): resposta maximamente plana

} // namespace

PPGBandpassFilter::PPGBandpassFilter(double low_cutoff_hz, double high_cutoff_hz, double sample_rate_hz)
    : highpass_(make_highpass(low_cutoff_hz, sample_rate_hz)),
      lowpass_(make_lowpass(high_cutoff_hz, sample_rate_hz)) {}

PPGBandpassFilter::BiquadState PPGBandpassFilter::make_lowpass(double cutoff_hz, double sample_rate_hz) {
    double w0 = 2.0 * PI * cutoff_hz / sample_rate_hz;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * BUTTERWORTH_Q);
    double a0 = 1.0 + alpha;

    return BiquadState{
        ((1.0 - cos_w0) / 2.0) / a0,
        (1.0 - cos_w0) / a0,
        ((1.0 - cos_w0) / 2.0) / a0,
        (-2.0 * cos_w0) / a0,
        (1.0 - alpha) / a0,
    };
}

PPGBandpassFilter::BiquadState PPGBandpassFilter::make_highpass(double cutoff_hz, double sample_rate_hz) {
    double w0 = 2.0 * PI * cutoff_hz / sample_rate_hz;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * BUTTERWORTH_Q);
    double a0 = 1.0 + alpha;

    return BiquadState{
        ((1.0 + cos_w0) / 2.0) / a0,
        (-(1.0 + cos_w0)) / a0,
        ((1.0 + cos_w0) / 2.0) / a0,
        (-2.0 * cos_w0) / a0,
        (1.0 - alpha) / a0,
    };
}

double PPGBandpassFilter::apply(BiquadState& state, double input) {
    double output = state.b0 * input + state.b1 * state.x1 + state.b2 * state.x2
                   - state.a1 * state.y1 - state.a2 * state.y2;

    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = output;

    return output;
}

double PPGBandpassFilter::process(uint32_t raw_sample) {
    double stage1 = apply(highpass_, static_cast<double>(raw_sample));
    double stage2 = apply(lowpass_, stage1);
    return stage2;
}
