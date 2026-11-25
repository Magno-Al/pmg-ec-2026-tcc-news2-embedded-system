#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

// Amostras já normalizadas (0..1) de uma curva ao vivo, endereçadas por um índice absoluto que
// só cresce. A fonte do sensor escreve; o WaveformItem lê com atraso fixo, interpolando, para
// varrer a tela suavemente mesmo com o sensor entregando as amostras em rajadas. A escrita vem da
// thread de aquisição e a leitura da thread da interface: todo acesso passa pelo mutex.
class LiveSignalBuffer
{
public:
    enum class Channel
    {
        Plethysmography,
        Respiration
    };

    static LiveSignalBuffer& forChannel(Channel channel);

    explicit LiveSignalBuffer(int capacity);

    void configure(double sampleRateHz);
    void append(double normalizedValue);

    bool hasData() const;
    double sampleRateHz() const;
    // índice absoluto da próxima amostra a ser escrita
    int64_t writtenCount() const;
    double valueAt(double absoluteIndex) const;

private:
    mutable std::mutex m_mutex;
    std::vector<double> m_samples;
    int64_t m_writtenCount = 0;
    double m_sampleRateHz = 25.0;
};
