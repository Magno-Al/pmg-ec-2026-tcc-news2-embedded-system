#include "LiveSignalBuffer.h"

#include <algorithm>
#include <cmath>

namespace
{

// ~5 s de pletismografia a 100 Hz: acima do atraso máximo de leitura da curva (3 s, no
// WaveformItem)
constexpr int BufferCapacity = 512;

}

LiveSignalBuffer& LiveSignalBuffer::plethysmography()
{
    static LiveSignalBuffer buffer(BufferCapacity);
    return buffer;
}

LiveSignalBuffer::LiveSignalBuffer(int capacity)
    : m_samples(capacity, 0.5)
{
}

void LiveSignalBuffer::configure(double sampleRateHz)
{
    m_sampleRateHz = sampleRateHz;
}

void LiveSignalBuffer::append(double normalizedValue)
{
    m_samples[m_writtenCount % int64_t(m_samples.size())] = normalizedValue;
    ++m_writtenCount;
}

double LiveSignalBuffer::valueAt(double absoluteIndex) const
{
    if(m_writtenCount == 0)
    {
        return 0.5;
    }
    const int64_t capacity = int64_t(m_samples.size());
    const double oldest = double(std::max<int64_t>(0, m_writtenCount - capacity));
    const double newest = double(m_writtenCount - 1);
    const double clamped = std::clamp(absoluteIndex, oldest, newest);
    const int64_t lower = int64_t(std::floor(clamped));
    const int64_t upper = std::min<int64_t>(lower + 1, m_writtenCount - 1);
    const double fraction = clamped - double(lower);
    const double lowerValue = m_samples[lower % capacity];
    const double upperValue = m_samples[upper % capacity];
    return lowerValue + (upperValue - lowerValue) * fraction;
}
