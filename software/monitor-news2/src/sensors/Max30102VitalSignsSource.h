#pragma once

#include "devices/VitalSignsSource.h"
#include "sensors/Ds18b20Thermometer.h"

#include <QThread>

class Max30102Acquisition;

// Fonte de sinais vitais real, na thread principal. O PPG (FC, SpO₂, FR, contato do dedo e
// curvas) vem da Max30102Acquisition, que roda numa thread própria; a temperatura vem do DS18B20
// (Ds18b20Thermometer), a cada 10 s, na thread dele, independente do dedo. Os sinais das duas
// threads chegam aqui enfileirados pelo Qt e seguem para o MonitorController na thread principal.
class Max30102VitalSignsSource : public VitalSignsSource
{
    Q_OBJECT

public:
    explicit Max30102VitalSignsSource(QObject* parent = nullptr);
    // para a thread de aquisição e espera ela terminar: o sensor é fechado lá
    ~Max30102VitalSignsSource() override;

    void start() override;

private:
    QThread m_acquisitionThread;
    // sem pai: pertence à thread de aquisição e é apagado quando ela termina
    Max30102Acquisition* m_acquisition;
    Ds18b20Thermometer m_thermometer;
};
