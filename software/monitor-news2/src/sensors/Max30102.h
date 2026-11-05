#pragma once

#include "core/PpgProcessor.h"

#include <QString>

#include <cstdint>
#include <vector>

// Driver do MAX30102 pela /dev/i2c-N (i2c-dev, sem driver IIO do kernel). Configura o modo
// SpO₂ (vermelho + IR) a 100 amostras/s com média de 4 na FIFO, ou seja, 25 amostras/s
// entregues, e lê a FIFO por polling: o pino INT não é necessário.
class Max30102
{
public:
    explicit Max30102(QString devicePath = QStringLiteral("/dev/i2c-1"));
    ~Max30102();

    Max30102(const Max30102&) = delete;
    Max30102& operator=(const Max30102&) = delete;

    // abre o barramento, confere o part id e configura o sensor
    bool open();
    void close();
    bool isOpen() const { return m_fileDescriptor >= 0; }
    QString errorString() const { return m_errorString; }
    // sem descritor para esperar por evento: a FIFO do chip é consultada periodicamente
    int notificationDescriptor() const { return -1; }

    // false em falha de I2C (sensor desconectado, por exemplo); as amostras lidas antes da
    // falha ficam em samples
    // fullRateSamples existe só pela interface comum com o Max30102Iio: aqui o chip já entrega
    // 25 Hz (média de 4 na FIFO) e não há amostras de taxa maior
    bool readAvailableSamples(
        std::vector<PpgSample>& samples,
        std::vector<PpgSample>* fullRateSamples = nullptr
    );

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t* destination, uint16_t length);
    bool fail(const QString& message);

    QString m_devicePath;
    int m_fileDescriptor = -1;
    QString m_errorString;
};
