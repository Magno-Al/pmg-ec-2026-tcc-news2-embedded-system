#pragma once

#include "core/PipelineVitalsEstimator.h"
#include "core/PpgProcessor.h"
#include "devices/VitalSignsSource.h"
#include "sensors/Ds18b20Thermometer.h"
#include "sensors/Max30102.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include <memory>

// Fonte de sinais vitais real: MAX30102 → PpgProcessor. O chip é lido pela /dev/i2c-1
// (i2c-dev, em espaço de usuário), consultando a FIFO a cada 40 ms. As curvas vão para os
// LiveSignalBuffer a cada amostra e FC/SpO₂/FR são publicadas uma vez por segundo, com 0
// quando ainda não há leitura. MONITOR_NEWS2_PPG_ALGORITHM decide quem calcula:
// PpgProcessor, PipelineVitalsEstimator, os dois (FC/SpO₂ do pipeline, FR do PpgProcessor) ou
// o PpgProcessor com a FC refinada pelo pipeline sobre as amostras de 100 Hz do sensor (o
// sensor entrega 100 Hz e a média de 4, a 25 Hz); o contato do dedo e as curvas são sempre do
// PpgProcessor. FC e FR são corrigidas pela taxa real do sensor (~1% abaixo da nominal).
// Amostras perdidas na FIFO do chip vão para o log e para o traço. A temperatura vem do DS18B20
// (Ds18b20Thermometer), a cada 10 s, independente do dedo.
class Max30102VitalSignsSource : public VitalSignsSource
{
    Q_OBJECT

public:
    explicit Max30102VitalSignsSource(QObject* parent = nullptr);

    void start() override;

private:
    void openSensor();
    void poll();
    void publishReading();
    void onFingerContactChanged();
    void handleSensorFailure(const QString& reason);
    void trackSampleRate(size_t newSampleCount);
    double sampleRateCorrection() const;

    Max30102 m_sensor;
    QElapsedTimer m_sinceLastSample;
    PpgProcessor m_processor;
#ifdef MONITOR_NEWS2_PPG_PIPELINE
    PipelineVitalsEstimator m_pipeline;
#endif
#ifdef MONITOR_NEWS2_PPG_REFINED_HEART_RATE
    PipelineVitalsEstimator m_refinedHeartRate{
        Max30102::FullRateHz, PipelineVitalsEstimator::Outputs::HeartRateOnly
    };
#endif
    // último total de amostras perdidas já avisado no log
    uint64_t m_reportedLostSampleCount = 0;
    // depuração: com MONITOR_NEWS2_TRACE=<arquivo>, grava cada amostra de 25 Hz (bruto, contato,
    // valores das curvas) e cada publicação (FC/SpO₂/FR e suas fontes)
    std::unique_ptr<QFile> m_traceFile;
    std::unique_ptr<QTextStream> m_trace;
    QElapsedTimer m_traceClock;
    // taxa real: amostras (25 Hz) contadas desde o primeiro lote após abrir o sensor
    QElapsedTimer m_sampleRateClock;
    long m_samplesSinceFirstBatch = 0;
    QTimer m_pollTimer;
    QTimer m_publishTimer;
    QTimer m_reconnectTimer;
    Ds18b20Thermometer m_thermometer;
};
