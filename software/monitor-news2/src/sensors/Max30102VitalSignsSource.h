#pragma once

#include "core/FingerContactDetector.h"
#include "core/PipelineVitalsEstimator.h"
#include "core/PlethysmographyDisplay.h"
#include "devices/VitalSignsSource.h"
#include "sensors/Ds18b20Thermometer.h"
#include "sensors/Max30102.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include <memory>

// Fonte de sinais vitais real: MAX30102 → PipelineVitalsEstimator (ppg-pipeline), a 100 Hz. O
// chip é lido pela /dev/i2c-1 (i2c-dev, em espaço de usuário), consultando a FIFO a cada 40 ms.
// O FingerContactDetector decide se há dedo; só com dedo as amostras vão para o estimador, que é
// zerado a cada troca de contato. A curva de pletismografia (PlethysmographyDisplay, só
// apresentação) vai para o LiveSignalBuffer a cada amostra; não há curva de respiração. FC,
// SpO₂ e FR são publicadas uma vez por segundo, com 0 quando o estimador não tem leitura (antes
// da primeira janela ou com a janela reprovada). FC e FR são corrigidas pela taxa real do
// sensor (~1% abaixo da nominal). Amostras perdidas na FIFO do chip vão para o log e para o
// traço. A temperatura vem do DS18B20 (Ds18b20Thermometer), a cada 10 s, independente do dedo.
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
    FingerContactDetector m_fingerContact{Max30102::FullRateHz};
    PipelineVitalsEstimator m_pipeline{Max30102::FullRateHz};
    PlethysmographyDisplay m_plethysmographyDisplay{Max30102::FullRateHz};
    // último total de amostras perdidas já avisado no log
    uint64_t m_reportedLostSampleCount = 0;
    // depuração: com MONITOR_NEWS2_TRACE=<arquivo>, grava cada amostra de 100 Hz (bruto,
    // contato, valor da curva) e cada publicação (FC/SpO₂/FR, correção, amostras perdidas)
    std::unique_ptr<QFile> m_traceFile;
    std::unique_ptr<QTextStream> m_trace;
    QElapsedTimer m_traceClock;
    // taxa real: amostras (100 Hz) contadas desde o primeiro lote após abrir o sensor
    QElapsedTimer m_sampleRateClock;
    long m_samplesSinceFirstBatch = 0;
    QTimer m_pollTimer;
    QTimer m_publishTimer;
    QTimer m_reconnectTimer;
    Ds18b20Thermometer m_thermometer;
};
