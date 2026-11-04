#include "MockVitalSignsSource.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr int ReadingIntervalMilliseconds = 2000;
constexpr int DeteriorationRespiratoryCeiling = 25;

}

MockVitalSignsSource::MockVitalSignsSource(
    const VitalSigns& target,
    int deteriorationIntervalSeconds,
    QObject* parent
)
    : VitalSignsSource{parent}
    , m_target(target)
    , m_deteriorationIntervalSeconds(deteriorationIntervalSeconds)
    , m_random(QRandomGenerator::securelySeeded())
{
    m_readingTimer.setInterval(ReadingIntervalMilliseconds);
    connect(&m_readingTimer, &QTimer::timeout, this, &MockVitalSignsSource::emitReading);
}

void MockVitalSignsSource::start()
{
    m_sinceLastDeterioration.start();
    m_readingTimer.start();
    emit fingerContactChanged(true);
    emitReading();
}

void MockVitalSignsSource::emitReading()
{
    const bool shouldDeteriorate = m_deteriorationIntervalSeconds > 0
        && m_sinceLastDeterioration.elapsed() >= m_deteriorationIntervalSeconds * 1000LL
        && m_target.respiratoryRate < DeteriorationRespiratoryCeiling;
    if(shouldDeteriorate)
    {
        ++m_target.respiratoryRate;
        m_sinceLastDeterioration.restart();
    }

    const int heartRate = m_target.heartRate + m_random.bounded(-2, 3);
    const int saturationDip = m_random.generateDouble() < 0.25 ? 1 : 0;
    const int oxygenSaturation = std::min(100, m_target.oxygenSaturation - saturationDip);
    const int respiratoryJitter = m_random.generateDouble() < 0.3 ? 1 : 0;
    const int respiratoryRate = m_target.respiratoryRate + respiratoryJitter;
    const double temperatureNoise = (m_random.generateDouble() - 0.5) * 0.12;
    const double temperature = std::round((m_target.temperature + temperatureNoise) * 10.0) / 10.0;

    emit pulseOximetryMeasured(heartRate, oxygenSaturation, respiratoryRate);
    emit temperatureMeasured(temperature);
}
