#ifndef PPG_SPO2_HPP
#define PPG_SPO2_HPP

#include <optional>
#include "PPGBuffer.hpp"

// Calibração empírica linear SpO2 = a - b*R (ver README, seção 5).
constexpr double PPG_SPO2_CALIBRATION_A = 110.0;
constexpr double PPG_SPO2_CALIBRATION_B = 25.0;

constexpr double PPG_SPO2_MIN_PLAUSIBLE = 50.0;
constexpr double PPG_SPO2_MAX_PLAUSIBLE = 100.0;

// R = PI_vermelho / PI_ir, reaproveitando o PI de calculate_perfusion_index
// (PPGQuality.hpp). Retorna nullopt se o PI do IR for zero ou se o resultado
// sair de [PPG_SPO2_MIN_PLAUSIBLE, PPG_SPO2_MAX_PLAUSIBLE].
std::optional<double> calculate_spo2(PPGWindowView window);

#endif
