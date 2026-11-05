#include "Max30102VitalSignsSource.h"

#include "core/LiveSignalBuffer.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace
{

constexpr int PollIntervalMilliseconds = 40;
constexpr int PublishIntervalMilliseconds = 1000;
constexpr int ReconnectIntervalMilliseconds = 2000;
// no IIO um sensor desconectado não dá erro, as interrupções só param de chegar
constexpr int StalledSensorMilliseconds = 3000;
// O oscilador do MAX30102 fica ~1% abaixo da taxa nominal (24,7 Hz medidos contra 25), o que
// desloca FC e FR na mesma proporção. A taxa é medida pela inclinação amostras × tempo a partir
// do primeiro lote, o que elimina o atraso fixo de entrega do IIO (~300 ms), e só é aplicada
// depois de 20 s de medição; ±5% é o limite de plausibilidade.
constexpr qint64 SampleRateMeasureMilliseconds = 20000;
constexpr double MaximumSampleRateCorrection = 0.05;

int roundedOrZero(std::optional<double> value)
{
    return value ? int(std::lround(*value)) : 0;
}

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
    connect(
        &m_thermometer, &Ds18b20Thermometer::temperatureMeasured,
        this, &VitalSignsSource::temperatureMeasured
    );
}

void Max30102VitalSignsSource::start()
{
    const QString tracePath = qEnvironmentVariable("MONITOR_NEWS2_TRACE");
    if(!tracePath.isEmpty())
    {
        m_traceFile = std::make_unique<QFile>(tracePath);
        if(m_traceFile->open(QIODevice::WriteOnly | QIODevice::Text))
        {
            m_trace = std::make_unique<QTextStream>(m_traceFile.get());
            *m_trace << "tipo,ms,a,b,c,d,e,f,g\n"
                     << "# s,ms,red,ir,contato,curva_pleth,curva_resp(;)\n"
                     << "# p,ms,fc,spo2,fr,fc_processor,fc_refinada,fr_bruta,correcao\n";
            m_traceClock.start();
            qInfo().noquote() << "traço em" << tracePath;
        }
    }
    LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Plethysmography)
        .configure(PpgProcessor::SampleRateHz);
    LiveSignalBuffer::forChannel(LiveSignalBuffer::Channel::Respiration)
        .configure(PpgProcessor::RespirationSampleRateHz);
    emit fingerContactChanged(false);
    m_publishTimer.start();
    m_thermometer.start();
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
    m_sampleRateClock.invalidate();
    m_samplesSinceFirstBatch = 0;
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
#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
    std::vector<PpgSample> fullRateSamples;
    const bool isHealthy = m_sensor.readAvailableSamples(samples, &fullRateSamples);
#else
    const bool isHealthy = m_sensor.readAvailableSamples(samples);
#endif
    if(!samples.empty())
    {
        m_sinceLastSample.restart();
        trackSampleRate(samples.size());
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
        if(m_trace)
        {
            QStringList respiration;
            for(double respirationValue : output.respirationDisplay)
            {
                respiration << QString::number(respirationValue, 'f', 3);
            }
            *m_trace << "s," << m_traceClock.elapsed() << ',' << sample.red << ','
                     << sample.infrared << ',' << int(m_processor.hasFingerContact()) << ','
                     << (output.plethysmographyDisplay
                             ? QString::number(*output.plethysmographyDisplay, 'f', 3)
                             : QString())
                     << ',' << respiration.join(';') << '\n';
        }
    }

#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
    if(m_processor.hasFingerContact())
    {
        for(const PpgSample& sample : fullRateSamples)
        {
            m_refinedHeartRate.addSample(sample);
        }
    }
#endif

    if(!isHealthy)
    {
        handleSensorFailure(m_sensor.errorString());
    }
}

void Max30102VitalSignsSource::trackSampleRate(size_t newSampleCount)
{
    if(!m_sampleRateClock.isValid())
    {
        m_sampleRateClock.start();
        return;
    }
    m_samplesSinceFirstBatch += long(newSampleCount);
}

double Max30102VitalSignsSource::sampleRateCorrection() const
{
    if(!m_sampleRateClock.isValid()
       || m_sampleRateClock.elapsed() < SampleRateMeasureMilliseconds)
    {
        return 1.0;
    }
    const double measuredRateHz =
        m_samplesSinceFirstBatch / (m_sampleRateClock.elapsed() / 1000.0);
    const double correction = measuredRateHz / PpgProcessor::SampleRateHz;
    return std::clamp(
        correction, 1.0 - MaximumSampleRateCorrection, 1.0 + MaximumSampleRateCorrection
    );
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
    std::optional<double> heartRate;
    if(const auto value = cardiacEstimator.heartRate())
    {
        heartRate = *value;
    }
#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
    // a FC do pipeline a 100 Hz substitui a do PpgProcessor assim que fecha (~35 s de contato)
    if(const auto refined = m_refinedHeartRate.preciseHeartRate())
    {
        heartRate = refined;
    }
#endif
    std::optional<double> respiratoryRate;
    if(const auto value = respiratoryEstimator.respiratoryRate())
    {
        respiratoryRate = *value;
    }
    const double correction = sampleRateCorrection();
    if(heartRate)
    {
        *heartRate *= correction;
    }
    if(respiratoryRate)
    {
        *respiratoryRate *= correction;
    }
    emit pulseOximetryMeasured(
        roundedOrZero(heartRate),
        cardiacEstimator.oxygenSaturation().value_or(0),
        roundedOrZero(respiratoryRate)
    );
    if(m_trace)
    {
#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
        const double refined = m_refinedHeartRate.preciseHeartRate().value_or(0.0);
#else
        const double refined = 0.0;
#endif
        *m_trace << "p," << m_traceClock.elapsed() << ',' << roundedOrZero(heartRate) << ','
                 << cardiacEstimator.oxygenSaturation().value_or(0) << ','
                 << roundedOrZero(respiratoryRate) << ','
                 << m_processor.heartRate().value_or(0) << ',' << refined << ','
                 << respiratoryEstimator.respiratoryRate().value_or(0) << ',' << correction
                 << '\n';
        m_trace->flush();
    }
}

void Max30102VitalSignsSource::onFingerContactChanged()
{
    const bool hasFingerContact = m_processor.hasFingerContact();
#ifdef MONITOR_NEWS2_PPG_PIPELINE
    m_pipeline.reset();
#endif
#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
    m_refinedHeartRate.reset();
#endif
    emit fingerContactChanged(hasFingerContact);
    if(hasFingerContact)
    {
        // a curva começa a andar já no contato, antes da primeira FC fechar
        publishReading();
        m_publishTimer.start();
    }
}
