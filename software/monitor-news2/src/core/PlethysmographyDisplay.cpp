#include "PlethysmographyDisplay.h"

#include <algorithm>
#include <cmath>

PlethysmographyDisplay::PlethysmographyDisplay(int sampleRateHz)
    : m_windowSamples(size_t(std::lround(WindowSeconds * sampleRateHz)))
{
}

void PlethysmographyDisplay::reset()
{
    m_recent.clear();
}

double PlethysmographyDisplay::add(double infraredFiltered)
{
    const double value = -infraredFiltered;
    m_recent.push_back(value);
    if(m_recent.size() > m_windowSamples)
    {
        m_recent.pop_front();
    }
    const auto [lowest, highest] = std::minmax_element(m_recent.begin(), m_recent.end());
    const double range = *highest - *lowest;
    if(range <= 1e-9)
    {
        return 0.5;
    }
    return 0.1 + 0.8 * (value - *lowest) / range;
}
