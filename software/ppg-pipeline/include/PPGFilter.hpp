#ifndef PPG_FILTER_HPP
#define PPG_FILTER_HPP

#include <cstdint>

constexpr double PPG_FILTER_LOW_CUTOFF_HZ = 0.5;
constexpr double PPG_FILTER_HIGH_CUTOFF_HZ = 4.0;

// Estado mantido entre chamadas de process(): amostras devem ser passadas em
// ordem, cada uma uma única vez.
class PPGBandpassFilter {
public:
    PPGBandpassFilter(double low_cutoff_hz, double high_cutoff_hz, double sample_rate_hz);

    double process(uint32_t raw_sample);

private:
    struct BiquadState {
        double b0, b1, b2, a1, a2; // coeficientes, já normalizados por a0
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0; // estado interno
    };

    static BiquadState make_highpass(double cutoff_hz, double sample_rate_hz);
    static BiquadState make_lowpass(double cutoff_hz, double sample_rate_hz);
    static double apply(BiquadState& state, double input);

    BiquadState highpass_;
    BiquadState lowpass_;
};

#endif
