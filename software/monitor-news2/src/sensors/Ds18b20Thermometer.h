#pragma once

#include <QObject>
#include <QString>
#include <QThread>

#include <optional>

// Temperatura do DS18B20 pelo driver w1-therm do kernel (1-Wire no GPIO 4). A leitura usa o
// módulo DS18B20 do ppg-pipeline e roda numa thread própria: cada leitura do w1_slave bloqueia
// ~750 ms enquanto o sensor converte, o que travaria a interface na thread principal. O
// barramento é varrido de novo a cada leitura, então o sensor pode ser ligado com o app aberto.
class Ds18b20Thermometer : public QObject
{
    Q_OBJECT

public:
    explicit Ds18b20Thermometer(
        QString devicesDirectory = QStringLiteral("/sys/bus/w1/devices"),
        int intervalMilliseconds = 10000,
        QObject* parent = nullptr
    );
    ~Ds18b20Thermometer() override;

    void start();

    // uma leitura síncrona; nullopt sem sensor, com CRC inválido ou no valor de reset
    static std::optional<double> readOnce(const QString& devicesDirectory);

signals:
    void temperatureMeasured(double celsius);

private:
    QString m_devicesDirectory;
    int m_intervalMilliseconds;
    QThread m_thread;
};
