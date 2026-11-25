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

// Fonte de sinais vitais real: MAX30102 → PpgProcessor + PipelineVitalsEstimator. O chip é lido
// pela /dev/i2c-1 (i2c-dev, em espaço de usuário), consultando a FIFO a cada 40 ms. As 100
// amostras/s do chip vão para o pipeline (MSPTD), que dá a FC; a média de cada 4 (25 Hz) vai
// para o PpgProcessor, que decide o contato do dedo, desenha as curvas e calcula SpO₂ e FR
// (e a FC dos primeiros ~35 s, até a janela do pipeline fechar). Escolha pela comparação na
// BIDMC e no sensor real (comparativo.md): o pipeline erra menos a FC, e o PpgProcessor a FR,
// sem o falso 6–7 irpm das ondas de Mayer. As curvas vão para os LiveSignalBuffer a cada amostra
// e FC/SpO₂/FR são publicadas uma vez por segundo, com 0 quando ainda não há leitura. FC e FR são
// corrigidas pela taxa real do sensor (~1% abaixo da nominal). Amostras perdidas na FIFO do chip
// vão para o log e para o traço. A temperatura vem do DS18B20 (Ds18b20Thermometer), a cada 10 s,
// independente do dedo.
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
    PipelineVitalsEstimator m_heartRateEstimator{
        Max30102::FullRateHz, PipelineVitalsEstimator::Outputs::HeartRateOnly
    };
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
