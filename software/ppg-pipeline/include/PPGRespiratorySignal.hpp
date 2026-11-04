#ifndef PPG_RESPIRATORY_SIGNAL_HPP
#define PPG_RESPIRATORY_SIGNAL_HPP

#include <optional>
#include <vector>

// values já chega reamostrada numa grade uniforme (sampling_period_s é sempre
// PPG_RESPIRATORY_RESAMPLE_PERIOD_S, ver PPGRespiratoryRate.hpp). Consumido por
// measure_respiratory_frequency_rpm, que é genérica quanto à modulação de origem.
struct PPGRespiratorySignal {
    std::vector<double> values;
    double sampling_period_s;
};

// Faixa de busca espectral / plausibilidade do resultado (ver README, seção 5,
// "FR, faixa de busca"). FR acima de 30 rpm não é detectável com essa faixa;
// resultado esperado nesse caso é nullopt, não um valor espúrio.
constexpr double PPG_RR_MIN_RPM = 6.0;
constexpr double PPG_RR_MAX_RPM = 30.0;
constexpr double PPG_RR_MIN_FREQUENCY_HZ = PPG_RR_MIN_RPM / 60.0; // 0.1 Hz
constexpr double PPG_RR_MAX_FREQUENCY_HZ = PPG_RR_MAX_RPM / 60.0; // 0.5 Hz

// Remove a média, aplica janela de Hamming, calcula o espectro via DFT direta e
// acha o pico de maior magnitude na faixa [PPG_RR_MIN_FREQUENCY_HZ,
// PPG_RR_MAX_FREQUENCY_HZ], convertido para rpm. Retorna nullopt se nenhum bin
// cair na faixa ou se todos os bins da faixa tiverem magnitude zero.
std::optional<double> measure_respiratory_frequency_rpm(const PPGRespiratorySignal& signal);

#endif
