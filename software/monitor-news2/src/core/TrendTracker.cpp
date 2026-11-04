#include "TrendTracker.h"

#include <algorithm>
#include <cmath>

TrendTracker::TrendTracker(double threshold, int windowSize)
    : m_threshold(threshold)
    , m_windowSize(windowSize)
{
}

void TrendTracker::addReading(double value, qint64 at)
{
    m_window.append(value);
    if(m_window.size() > m_windowSize)
    {
        m_window.removeFirst();
    }
    if(m_window.size() < m_windowSize)
    {
        return;
    }

    const double current = median();
    if(!m_hasLevel)
    {
        m_level = current;
        m_hasLevel = true;
        return;
    }

    const double difference = current - m_level;
    if(std::abs(difference) < m_threshold)
    {
        return;
    }
    const Direction direction = difference > 0 ? Direction::Rising : Direction::Falling;
    if(direction != m_direction)
    {
        m_direction = direction;
        m_since = at;
    }
    m_level = current;
}

double TrendTracker::median() const
{
    QVector<double> sorted = m_window;
    std::sort(sorted.begin(), sorted.end());
    const int middle = sorted.size() / 2;
    return sorted.size() % 2 == 1 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2.0;
}
