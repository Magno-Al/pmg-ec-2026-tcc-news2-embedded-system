#include "Max30102VitalSignsSource.h"

#include "core/LiveSignalBuffer.h"

#include <QDebug>

#include <vector>

namespace
{

constexpr int PollIntervalMilliseconds = 40;
constexpr int PublishIntervalMilliseconds = 1000;
constexpr int ReconnectIntervalMilliseconds = 2000;
// no IIO um sensor desconectado não dá erro, as interrupções só param de chegar
constexpr int StalledSensorMilliseconds = 3000;

}

Max30102VitalSignsSource::Max30102VitalSignsSource(QObject* parent)
    : VitalSignsSource{parent}
{
    m_pollTimer.setInterval(PollIntervalMilliseconds);
    connect(&m_pollTimer, &QTimer::timeout, this, &Max30102VitalSignsSource::poll);
    m_publishTimer.setInterval(PublishIntervalMilliseconds);
    connect(&m_publishTimer, &QTimer::timeout, this, &Max30102VitalSignsSource::publishReading);
    m_reconnectTimer.setInterval(ReconnectIntervalMilliseconds);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &Max30102VitalSignsSource::openSensor);
}

void Max30102VitalSignsSource::start()
{
    LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Plethysmography)
        .configure(PpgProcessor::SampleRateHz);
    LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Respiration)
        .configure(PpgProcessor::RespirationSampleRateHz);
    emit fingerContactChanged(false);
    m_publishTimer.start();
    openSensor();
}

void Max30102VitalSignsSource::openSensor()
{
    if(!m_sensor.open())
    {
        qWarning().noquote() << "MAX30102:" << m_sensor.errorString()
                             << "- nova tentativa em 2 s";
        m_reconnectTimer.start();
        return;
    }
    m_processor = PpgProcessor();
    m_sinceLastSample.start();
    const int descriptor = m_sensor.notificationDescriptor();
    if(descriptor >= 0)
    {
        qInfo() << "MAX30102: driver IIO do kernel, leitura por evento";
        m_dataNotifier = std::make_unique<QSocketNotifier>(descriptor, QSocketNotifier::Read);
        connect(m_dataNotifier.get(), &QSocketNotifier::activated,
                this, &Max30102VitalSignsSource::poll);
    }
    else
    {
        qInfo() << "MAX30102: i2c-dev em espaço de usuário, leitura a cada 40 ms";
        m_pollTimer.start();
    }
}

void Max30102VitalSignsSource::poll()
{
    std::vector<PpgSample> samples;
    const bool isHealthy = m_sensor.readAvailableSamples(samples);
    if(!samples.empty())
    {
        m_sinceLastSample.restart();
    }

    for(const PpgSample& sample : samples)
    {
        const PpgProcessor::Output output = m_processor.addSample(sample);
        if(output.hasFingerContactChanged)
        {
            onFingerContactChanged();
        }
#ifdef MONITOR_NEWS2_PPG_PIPELINE
        if(m_processor.hasFingerContact())
        {
            m_pipeline.addSample(sample);
        }
#endif
        if(output.plethysmographyDisplay)
        {
            LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Plethysmography)
                .append(*output.plethysmographyDisplay);
        }
        for(double respirationValue : output.respirationDisplay)
        {
            LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Respiration)
                .append(respirationValue);
        }
    }

    if(!isHealthy)
    {
        handleSensorFailure(m_sensor.errorString());
    }
}

void Max30102VitalSignsSource::handleSensorFailure(const QString& reason)
{
    qWarning().noquote() << "MAX30102:" << reason << "- reconectando";
    m_pollTimer.stop();
    m_dataNotifier.reset();
    m_sensor.close();
    if(m_processor.hasFingerContact())
    {
        m_processor = PpgProcessor();
        emit fingerContactChanged(false);
    }
    m_reconnectTimer.start();
}

void Max30102VitalSignsSource::publishReading()
{
    if(m_sensor.isOpen() && m_sinceLastSample.elapsed() > StalledSensorMilliseconds)
    {
        handleSensorFailure(QStringLiteral("nenhuma amostra há 3 s"));
        return;
    }
    if(!m_processor.hasFingerContact())
    {
        return;
    }
#if defined(MONITOR_NEWS2_PPG_HYBRID)
    const auto& cardiacEstimator = m_pipeline;
    const auto& respiratoryEstimator = m_processor;
#elif defined(MONITOR_NEWS2_PPG_PIPELINE)
    const auto& cardiacEstimator = m_pipeline;
    const auto& respiratoryEstimator = m_pipeline;
#else
    const auto& cardiacEstimator = m_processor;
    const auto& respiratoryEstimator = m_processor;
#endif
    emit pulseOximetryMeasured(
        cardiacEstimator.heartRate().value_or(0),
        cardiacEstimator.oxygenSaturation().value_or(0),
        respiratoryEstimator.respiratoryRate().value_or(0)
    );
}

void Max30102VitalSignsSource::onFingerContactChanged()
{
    const bool hasFingerContact = m_processor.hasFingerContact();
#ifdef MONITOR_NEWS2_PPG_PIPELINE
    m_pipeline.reset();
#endif
    emit fingerContactChanged(hasFingerContact);
    if(hasFingerContact)
    {
        // a curva começa a andar já no contato, antes da primeira FC fechar
        publishReading();
        m_publishTimer.start();
    }
}
