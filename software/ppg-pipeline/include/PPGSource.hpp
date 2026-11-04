#ifndef PPG_SOURCE_HPP
#define PPG_SOURCE_HPP

#include <optional>
#include "PPG.hpp"

// Metadados da fonte, conhecidos antes de começar a ler amostras (ver README,
// seção 4, "Formato de entrada"). has_red_channel é derivado da presença da
// coluna red_signal no CSV; os outros dois campos vêm do .meta.
struct PPGSourceInfo {
    int sample_rate_hz;
    bool has_red_channel;
    bool raw_intensity = true;
};

// Deve ser chamada uma única vez, antes da primeira leitura de amostra
// (read_ppg_sample). sample_rate_hz vem 0 se a fonte não pôde ser aberta ou não
// tinha metadados de taxa válidos -- main.cpp trata isso como erro fatal.
PPGSourceInfo open_ppg_source();

std::optional<PPGSample> read_ppg_sample();

#endif
