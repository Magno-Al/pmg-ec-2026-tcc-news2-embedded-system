#ifndef PPG_HPP
#define PPG_HPP

#include <cstdint>

struct PPGSample {
    uint32_t red;
    uint32_t ir;
};

struct PPGWindowSample {
    uint32_t red_raw;
    uint32_t ir_raw;
    double red_filtered;
    double ir_filtered;
};

enum class PPGChannel {
    Red,
    Ir,
};

// Usada só pelos testes para gerar sinais sintéticos; a taxa real da fonte
// vem de PPGSourceInfo::sample_rate_hz (ver README, seção 4).
constexpr int PPG_SAMPLE_RATE_HZ = 86;

// Constantes do método em segundos, convertidas em amostras em tempo de
// execução pela taxa real da fonte (ver README, seção 5, e PPGBuffer.hpp).
constexpr int PPG_WINDOW_HOP_SECONDS = 5;
constexpr int PPG_WINDOW_SECONDS_MAX = 60;
constexpr int PPG_WINDOW_SECONDS_HR_SPO2 = 30;

#endif
