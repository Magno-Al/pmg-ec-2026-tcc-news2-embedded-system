#pragma once

#include <QObject>

// Fronteira com os sensores. A implementação real lerá o MAX30102 (I2C, PPG: FC, SpO2, FR) e o
// DS18B20 (1-Wire, temperatura); são sensores independentes, por isso os sinais são separados.
class VitalSignsSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~VitalSignsSource() override = default;

    virtual void start() = 0;

signals:
    void pulseOximetryMeasured(int heartRate, int oxygenSaturation, int respiratoryRate);
    void temperatureMeasured(double celsius);
    void fingerContactChanged(bool hasFingerContact);
};
