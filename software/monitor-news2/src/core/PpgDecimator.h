#pragma once

#include "core/PpgProcessor.h"

#include <cstdint>
#include <vector>

// Média de blocos de N amostras consecutivas, sem sobreposição: transforma as 100 amostras/s do
// MAX30102 nas 25 amostras/s que o PpgProcessor espera (N = 4). Usado pelo Max30102 (i2c-dev).
// Um bloco incompleto no fim de uma leitura fica
// guardado e é completado pelas amostras da leitura seguinte: nenhuma amostra é descartada. A
// média é a divisão inteira da soma (trunca), como o MAX30102 faz na própria FIFO.
class PpgDecimator
{
public:
    explicit PpgDecimator(int factor);

    // descarta o bloco incompleto (sensor reaberto: as amostras antigas não continuam o sinal)
    void reset();

    // acrescenta uma amostra; quando ela fecha um bloco, a média do bloco vai para o fim de output
    void add(const PpgSample& sample, std::vector<PpgSample>& output);

    int factor() const { return m_factor; }
    // amostras guardadas do bloco ainda incompleto (0 a factor − 1)
    int pendingCount() const { return m_count; }

private:
    int m_factor;
    uint64_t m_redSum = 0;
    uint64_t m_infraredSum = 0;
    int m_count = 0;
};
