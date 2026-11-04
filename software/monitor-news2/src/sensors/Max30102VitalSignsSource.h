#pragma once

#include "core/PipelineVitalsEstimator.h"
#include "core/PpgProcessor.h"
#include "devices/VitalSignsSource.h"
#include "sensors/Ds18b20Thermometer.h"

#ifdef MONITOR_NEWS2_SENSOR_IIO
#include "sensors/Max30102Iio.h"
using Max30102Device = Max30102Iio;
#else
#include "sensors/Max30102.h"
using Max30102Device = Max30102;
#endif

#include <QElapsedTimer>
#include <QSocketNotifier>
#include <QTimer>

#include <memory>

// Fonte de sinais vitais real: MAX30102 → PpgProcessor. O acesso ao chip é escolhido na
// compilação (MONITOR_NEWS2_SENSOR_BACKEND): i2c-dev em espaço de usuário, consultando a FIFO
// a cada 40 ms, ou o driver IIO do kernel, que avisa por evento quando há amostras. O resto é
// igual: as curvas vão para os LiveSignalBuffer a cada amostra e FC/SpO₂/FR são publicadas
// uma vez por segundo, com 0 quando ainda não há leitura. MONITOR_NEWS2_PPG_ALGORITHM decide
// quem calcula: PpgProcessor, PipelineVitalsEstimator ou os dois (FC/SpO₂ do pipeline, FR do
// PpgProcessor); o contato do dedo e as curvas são sempre do PpgProcessor. A temperatura vem do
// DS18B20 (Ds18b20Thermometer), a cada 10 s, independente do dedo.
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

    Max30102Device m_sensor;
    std::unique_ptr<QSocketNotifier> m_dataNotifier;
    QElapsedTimer m_sinceLastSample;
    PpgProcessor m_processor;
#ifdef MONITOR_NEWS2_PPG_PIPELINE
    PipelineVitalsEstimator m_pipeline;
#endif
    QTimer m_pollTimer;
    QTimer m_publishTimer;
    QTimer m_reconnectTimer;
    Ds18b20Thermometer m_thermometer;
};
