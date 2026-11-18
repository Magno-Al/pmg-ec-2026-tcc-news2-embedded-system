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
// vigia: o barramento pode responder sem que a FIFO do chip receba amostras (chip reiniciado,
// por exemplo); sem amostra por 3 s, o sensor é reaberto e reconfigurado
constexpr int StalledSensorMilliseconds = 3000;
// O oscilador do MAX30102 fica ~1% abaixo da taxa nominal (24,7 Hz medidos contra 25, na
// configuração anterior do chip), o que desloca FC e FR na mesma proporção. A taxa é medida pela inclinação amostras × tempo a partir
// do primeiro lote, o que elimina o atraso fixo entre a amostra e a leitura, e só é aplicada
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
            *m_trace << "tipo,ms,a,b,c,d,e\n"
                     << "# s,ms,red,ir,contato,curva_pleth\n"
                     << "# p,ms,fc,spo2,fr,correcao,amostras_perdidas\n";
            m_traceClock.start();
            qInfo().noquote() << "traço em" << tracePath;
        }
    }
    LiveSignalBuffer::plethysmography().configure(Max30102::FullRateHz);
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
    m_fingerContact.reset();
    m_sinceLastSample.start();
    m_sampleRateClock.invalidate();
    m_samplesSinceFirstBatch = 0;
    qInfo() << "MAX30102: i2c-dev em espaço de usuário, leitura a cada 40 ms";
    m_pollTimer.start();
}

void Max30102VitalSignsSource::poll()
{
    std::vector<PPGSample> samples;
    const bool isHealthy = m_sensor.readAvailableSamples(samples);
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

    for(const PPGSample& sample : samples)
    {
        if(m_fingerContact.update(sample.ir))
        {
            onFingerContactChanged();
        }
        std::optional<double> plethysmography;
        if(m_fingerContact.hasFingerContact())
        {
            plethysmography = m_plethysmographyDisplay.add(m_pipeline.addSample(sample));
            LiveSignalBuffer::plethysmography().append(*plethysmography);
        }
        if(m_trace)
        {
            *m_trace << "s," << m_traceClock.elapsed() << ',' << sample.red << ',' << sample.ir
                     << ',' << int(m_fingerContact.hasFingerContact()) << ','
                     << (plethysmography ? QString::number(*plethysmography, 'f', 3) : QString())
                     << '\n';
        }
    }

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
    const double correction = measuredRateHz / Max30102::FullRateHz;
    return std::clamp(
        correction, 1.0 - MaximumSampleRateCorrection, 1.0 + MaximumSampleRateCorrection
    );
}

void Max30102VitalSignsSource::handleSensorFailure(const QString& reason)
{
    qWarning().noquote() << "MAX30102:" << reason << "- reconectando";
    m_pollTimer.stop();
    m_sensor.close();
    if(m_fingerContact.hasFingerContact())
    {
        m_fingerContact.reset();
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
    if(!m_fingerContact.hasFingerContact())
    {
        return;
    }
    // sem leitura do estimador (antes da primeira janela ou com a janela reprovada), o
    // parâmetro vai como 0: ausente, sem repetir o valor anterior
    std::optional<double> heartRate = m_pipeline.preciseHeartRate();
    std::optional<double> respiratoryRate;
    if(const auto value = m_pipeline.respiratoryRate())
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
    const int oxygenSaturation = m_pipeline.oxygenSaturation().value_or(0);
    emit pulseOximetryMeasured(
        roundedOrZero(heartRate), oxygenSaturation, roundedOrZero(respiratoryRate)
    );
    if(m_trace)
    {
        *m_trace << "p," << m_traceClock.elapsed() << ',' << roundedOrZero(heartRate) << ','
                 << oxygenSaturation << ',' << roundedOrZero(respiratoryRate) << ','
                 << correction << ',' << m_sensor.lostSampleCount() << '\n';
        m_trace->flush();
    }
}

void Max30102VitalSignsSource::onFingerContactChanged()
{
    const bool hasFingerContact = m_fingerContact.hasFingerContact();
    m_pipeline.reset();
    m_plethysmographyDisplay.reset();
    emit fingerContactChanged(hasFingerContact);
    if(hasFingerContact)
    {
        // a curva começa a andar já no contato, antes da primeira FC fechar
        publishReading();
        m_publishTimer.start();
    }
}
