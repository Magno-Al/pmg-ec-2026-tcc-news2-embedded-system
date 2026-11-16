#include "PpgDecimator.h"

#include <algorithm>

PpgDecimator::PpgDecimator(int factor)
    : m_factor(std::max(factor, 1))
{
}

void PpgDecimator::reset()
{
    m_redSum = 0;
    m_infraredSum = 0;
    m_count = 0;
}

void PpgDecimator::add(const PpgSample& sample, std::vector<PpgSample>& output)
{
    m_redSum += sample.red;
    m_infraredSum += sample.infrared;
    ++m_count;
    if(m_count < m_factor)
    {
        return;
    }
    PpgSample average;
    average.red = uint32_t(m_redSum / uint64_t(m_factor));
    average.infrared = uint32_t(m_infraredSum / uint64_t(m_factor));
    output.push_back(average);
    reset();
}
