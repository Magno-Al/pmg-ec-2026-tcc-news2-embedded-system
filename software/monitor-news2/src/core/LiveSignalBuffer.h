#pragma once

#include <cstdint>
#include <vector>

// Amostras já normalizadas (0..1) de uma curva ao vivo, endereçadas por um índice absoluto que
// só cresce. A fonte do sensor escreve; o WaveformItem lê com atraso fixo, interpolando, para
// varrer a tela suavemente mesmo com o sensor entregando as amostras em rajadas.
class LiveSignalBuffer
{
public:
    enum class Channel
    {
        Plethysmography,
        Respiration
    };

    static LiveSignalBuffer& forChannel(Channel channel);

    void configure(double sampleRateHz);
    void append(double normalizedValue);

    bool hasData() const { return m_writtenCount > 0; }
    double sampleRateHz() const { return m_sampleRateHz; }
    // índice absoluto da próxima amostra a ser escrita
    int64_t writtenCount() const { return m_writtenCount; }
    double valueAt(double absoluteIndex) const;

private:
    explicit LiveSignalBuffer(int capacity);

    std::vector<double> m_samples;
    int64_t m_writtenCount = 0;
    double m_sampleRateHz = 25.0;
};
