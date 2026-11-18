#pragma once

#include <PPG.hpp>

#include <QString>

#include <cstdint>
#include <vector>

// Driver do MAX30102 pela /dev/i2c-N (i2c-dev, em espaço de usuário). Configura o modo SpO₂
// (vermelho + IR) a 400 amostras/s com média de 4 na FIFO, ou seja, 100 amostras/s entregues,
// e lê a FIFO por polling: o pino INT não é usado. Entrega as amostras de 100 Hz sem alteração,
// no tipo de amostra do ppg-pipeline (PPGSample: contagens brutas de 18 bits).
class Max30102
{
public:
    // amostras/s que o chip entrega (400 Hz internos, média de 4 na FIFO)
    static constexpr int FullRateHz = 100;

    explicit Max30102(QString devicePath = QStringLiteral("/dev/i2c-1"));
    ~Max30102();

    Max30102(const Max30102&) = delete;
    Max30102& operator=(const Max30102&) = delete;

    // abre o barramento, confere o part id e configura o sensor
    bool open();
    void close();
    bool isOpen() const { return m_fileDescriptor >= 0; }
    QString errorString() const { return m_errorString; }

    // acrescenta a samples as amostras de 100 Hz prontas na FIFO. false em falha de I2C
    // (sensor desconectado, por exemplo); as amostras lidas antes da falha ficam no vetor.
    bool readAvailableSamples(std::vector<PPGSample>& samples);

    // Amostras de 100 Hz que o chip descartou com a FIFO cheia (contador de overflow), somadas
    // desde a criação do objeto, inclusive entre reconexões. A FIFO cobre 320 ms a 100 Hz.
    uint64_t lostSampleCount() const { return m_lostSampleCount; }

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t* destination, uint16_t length);
    bool fail(const QString& message);

    QString m_devicePath;
    int m_fileDescriptor = -1;
    QString m_errorString;
    uint64_t m_lostSampleCount = 0;
};
