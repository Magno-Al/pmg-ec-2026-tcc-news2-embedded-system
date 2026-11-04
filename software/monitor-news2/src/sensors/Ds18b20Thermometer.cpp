#include "Ds18b20Thermometer.h"

#include <DS18B20.hpp>

#include <QDebug>
#include <QTimer>

#include <memory>
#include <utility>

namespace
{

// O DS18B20 devolve 85,000 °C quando é lido antes de completar uma conversão (valor de reset
// do registrador): não é medida.
constexpr double PowerOnResetCelsius = 85.0;

}

Ds18b20Thermometer::Ds18b20Thermometer(
    QString devicesDirectory,
    int intervalMilliseconds,
    QObject* parent
)
    : QObject{parent}
    , m_devicesDirectory(std::move(devicesDirectory))
    , m_intervalMilliseconds(intervalMilliseconds)
{
    m_thread.setObjectName(QStringLiteral("ds18b20"));
}

Ds18b20Thermometer::~Ds18b20Thermometer()
{
    m_thread.quit();
    m_thread.wait();
}

void Ds18b20Thermometer::start()
{
    if(m_thread.isRunning())
    {
        return;
    }
    // o timer vive na thread de leitura; o sinal emitido de lá chega enfileirado a quem está
    // na thread principal
    auto* timer = new QTimer;
    timer->setInterval(m_intervalMilliseconds);
    timer->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, timer, &QObject::deleteLater);

    const QString devicesDirectory = m_devicesDirectory;
    // avisa uma vez quando o sensor some e uma vez quando volta, não a cada leitura
    const auto wasMissing = std::make_shared<bool>(false);
    const auto read = [this, devicesDirectory, wasMissing]() {
        const std::optional<double> celsius = readOnce(devicesDirectory);
        if(!celsius)
        {
            if(!*wasMissing)
            {
                qWarning() << "DS18B20: sem leitura em" << devicesDirectory;
            }
            *wasMissing = true;
            return;
        }
        if(*wasMissing)
        {
            qInfo() << "DS18B20: leitura restabelecida";
        }
        *wasMissing = false;
        emit temperatureMeasured(*celsius);
    };
    connect(timer, &QTimer::timeout, timer, read);
    connect(&m_thread, &QThread::started, timer, [timer, read]() {
        read();
        timer->start();
    });
    m_thread.start();
}

std::optional<double> Ds18b20Thermometer::readOnce(const QString& devicesDirectory)
{
    const std::optional<std::string> w1Slave =
        find_ds18b20_w1_slave(devicesDirectory.toStdString());
    if(!w1Slave)
    {
        return std::nullopt;
    }
    const std::optional<double> celsius = read_ds18b20_w1_slave(*w1Slave);
    if(!celsius || *celsius == PowerOnResetCelsius)
    {
        return std::nullopt;
    }
    return celsius;
}
