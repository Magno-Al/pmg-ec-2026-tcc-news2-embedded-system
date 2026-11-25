#include "Max30102Acquisition.h"

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
// vigia: o barramento pode responder sem que a FIFO do chip receba amostras (chip reiniciado,
// por exemplo); sem amostra por 3 s, o sensor é reaberto e reconfigurado
constexpr int StalledSensorMilliseconds = 3000;
// O oscilador do MAX30102 fica ~1% abaixo da taxa nominal (24,7 Hz medidos contra 25), o que
// desloca FC e FR na mesma proporção. A taxa é medida pela inclinação amostras × tempo a partir
// do primeiro lote, o que elimina o atraso fixo entre a amostra e a leitura, e só é aplicada
// depois de 20 s de medição; ±5% é o limite de plausibilidade.
constexpr qint64 SampleRateMeasureMilliseconds = 20000;
constexpr double MaximumSampleRateCorrection = 0.05;

int roundedOrZero(std::optional<double> value)
{
    return value ? int(std::lround(*value)) : 0;
}

}

Max30102Acquisition::Max30102Acquisition(QObject* parent)
    : QObject{parent}
{
    // a leitura a cada 40 ms tem só 320 ms de folga na FIFO do chip: sem a folga de ~5% do
    // timer padrão do Qt
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(PollIntervalMilliseconds);
    connect(&m_pollTimer, &QTimer::timeout, this, &Max30102Acquisition::poll);
    m_publishTimer.setInterval(PublishIntervalMilliseconds);
    connect(&m_publishTimer, &QTimer::timeout, this, &Max30102Acquisition::publishReading);
    m_reconnectTimer.setInterval(ReconnectIntervalMilliseconds);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &Max30102Acquisition::openSensor);
}

void Max30102Acquisition::start()
{
    const QString tracePath = qEnvironmentVariable("MONITOR_NEWS2_TRACE");
    if(!tracePath.isEmpty())
    {
        m_traceFile = std::make_unique<QFile>(tracePath);
        if(m_traceFile->open(QIODevice::WriteOnly | QIODevice::Text))
        {
            m_trace = std::make_unique<QTextStream>(m_traceFile.get());
            *m_trace << "tipo,ms,a,b,c,d,e,f,g,h\n"
                     << "# s,ms,red,ir,contato,curva_pleth,curva_resp(;)\n"
                     << "# p,ms,fc,spo2,fr,fc_processor,fc_pipeline,fr_bruta,correcao,"
                        "amostras_perdidas\n";
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
    openSensor();
}

void Max30102Acquisition::openSensor()
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
    qInfo() << "MAX30102: i2c-dev em espaço de usuário, leitura a cada 40 ms";
    m_pollTimer.start();
}

void Max30102Acquisition::poll()
{
    std::vector<PpgSample> samples;
    std::vector<PpgSample> fullRateSamples;
    const bool isHealthy = m_sensor.readAvailableSamples(samples, &fullRateSamples);
    if(!samples.empty())
    {
        m_sinceLastSample.restart();
        trackSampleRate(samples.size());
    }
    if(m_sensor.lostSampleCount() > m_reportedLostSampleCount)
    {
        qWarning().noquote() << "MAX30102: FIFO do chip transbordou,"
                             << m_sensor.lostSampleCount() - m_reportedLostSampleCount
                             << "amostras de 100 Hz perdidas (total"
                             << m_sensor.lostSampleCount() << ")";
        m_reportedLostSampleCount = m_sensor.lostSampleCount();
    }

    for(const PpgSample& sample : samples)
    {
        const PpgProcessor::Output output = m_processor.addSample(sample);
        if(output.hasFingerContactChanged)
        {
            onFingerContactChanged();
        }
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

    if(m_processor.hasFingerContact())
    {
        for(const PpgSample& sample : fullRateSamples)
        {
            m_heartRateEstimator.addSample(sample);
        }
    }

    if(!isHealthy)
    {
        handleSensorFailure(m_sensor.errorString());
    }
}

void Max30102Acquisition::trackSampleRate(size_t newSampleCount)
{
    if(!m_sampleRateClock.isValid())
    {
        m_sampleRateClock.start();
        return;
    }
    m_samplesSinceFirstBatch += long(newSampleCount);
}

double Max30102Acquisition::sampleRateCorrection() const
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

void Max30102Acquisition::handleSensorFailure(const QString& reason)
{
    qWarning().noquote() << "MAX30102:" << reason << "- reconectando";
    m_pollTimer.stop();
    m_sensor.close();
    if(m_processor.hasFingerContact())
    {
        m_processor = PpgProcessor();
        emit fingerContactChanged(false);
    }
    m_reconnectTimer.start();
}

void Max30102Acquisition::publishReading()
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
    std::optional<double> heartRate;
    if(const auto value = m_processor.heartRate())
    {
        heartRate = *value;
    }
    // a FC do pipeline a 100 Hz substitui a do PpgProcessor assim que fecha (~35 s de contato)
    if(const auto precise = m_heartRateEstimator.preciseHeartRate())
    {
        heartRate = precise;
    }
    std::optional<double> respiratoryRate;
    if(const auto value = m_processor.respiratoryRate())
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
        m_processor.oxygenSaturation().value_or(0),
        roundedOrZero(respiratoryRate)
    );
    if(m_trace)
    {
        const double precise = m_heartRateEstimator.preciseHeartRate().value_or(0.0);
        *m_trace << "p," << m_traceClock.elapsed() << ',' << roundedOrZero(heartRate) << ','
                 << m_processor.oxygenSaturation().value_or(0) << ','
                 << roundedOrZero(respiratoryRate) << ','
                 << m_processor.heartRate().value_or(0) << ',' << precise << ','
                 << m_processor.respiratoryRate().value_or(0) << ',' << correction
                 << ',' << m_sensor.lostSampleCount() << '\n';
        m_trace->flush();
    }
}

void Max30102Acquisition::onFingerContactChanged()
{
    const bool hasFingerContact = m_processor.hasFingerContact();
    m_heartRateEstimator.reset();
    emit fingerContactChanged(hasFingerContact);
    if(hasFingerContact)
    {
        // a curva começa a andar já no contato, antes da primeira FC fechar
        publishReading();
        m_publishTimer.start();
    }
}
