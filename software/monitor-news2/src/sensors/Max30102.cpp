#include "Max30102.h"

#include "Max30102Fifo.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QThread>

#include <cerrno>
#include <cstring>
#include <utility>

namespace
{

constexpr uint16_t SensorAddress = 0x57;
constexpr uint8_t ExpectedPartId = 0x15;
// 100 amostras/s do chip -> 25 amostras/s do PpgProcessor
constexpr int DecimationFactor = Max30102::FullRateHz / 25;

constexpr uint8_t RegisterFifoWritePointer = 0x04;
constexpr uint8_t RegisterFifoOverflowCounter = 0x05;
constexpr uint8_t RegisterFifoReadPointer = 0x06;
constexpr uint8_t RegisterFifoData = 0x07;
constexpr uint8_t RegisterFifoConfig = 0x08;
constexpr uint8_t RegisterModeConfig = 0x09;
constexpr uint8_t RegisterSpo2Config = 0x0A;
constexpr uint8_t RegisterLedRedAmplitude = 0x0C;
constexpr uint8_t RegisterLedInfraredAmplitude = 0x0D;
constexpr uint8_t RegisterPartId = 0xFF;

constexpr uint8_t ModeReset = 0x40;
constexpr uint8_t ModeShutdown = 0x80;
constexpr uint8_t ModeSpo2 = 0x03;
// média de 4 amostras na FIFO + rollover
constexpr uint8_t FifoAverageFourWithRollover = 0x50;
// SPO2_CONFIG = 0b0'01'011'11: faixa do ADC 4096 nA (01), 400 amostras/s (011), pulso de
// 411 µs, 18 bits (11). Com a média de 4 da FIFO, o chip entrega 100 amostras/s — a mesma
// configuração do driver max30102 do Linux (drivers/iio/health/max30102.c). Conferir no
// datasheet (tabela de configurações permitidas no modo SpO₂: taxa × largura de pulso) que
// 400 Hz com pulso de 411 µs é aceito.
constexpr uint8_t Spo2Range4096Rate400Pulse411 = 0x2F;
// ~7 mA: com o dedo, IR na casa de 10⁵ contagens, longe da saturação (2¹⁸)
constexpr uint8_t LedAmplitude = 0x24;

QString systemError()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

}

Max30102::Max30102(QString devicePath)
    : m_devicePath(std::move(devicePath))
    , m_decimator(DecimationFactor)
{
}

Max30102::~Max30102()
{
    close();
}

bool Max30102::open()
{
    close();
    m_fileDescriptor = ::open(m_devicePath.toLocal8Bit().constData(), O_RDWR);
    if(m_fileDescriptor < 0)
    {
        return fail(QStringLiteral("não abriu %1: %2").arg(m_devicePath, systemError()));
    }

    uint8_t partId = 0;
    if(!readRegisters(RegisterPartId, &partId, 1))
    {
        return false;
    }
    if(partId != ExpectedPartId)
    {
        return fail(QStringLiteral("part id 0x%1 em 0x57, esperado 0x15 (MAX30102)")
                        .arg(partId, 2, 16, QLatin1Char('0')));
    }

    if(!writeRegister(RegisterModeConfig, ModeReset))
    {
        return false;
    }
    QThread::msleep(100);
    const bool isConfigured = writeRegister(RegisterFifoConfig, FifoAverageFourWithRollover)
        && writeRegister(RegisterModeConfig, ModeSpo2)
        && writeRegister(RegisterSpo2Config, Spo2Range4096Rate400Pulse411)
        && writeRegister(RegisterLedRedAmplitude, LedAmplitude)
        && writeRegister(RegisterLedInfraredAmplitude, LedAmplitude)
        && writeRegister(RegisterFifoWritePointer, 0)
        && writeRegister(RegisterFifoOverflowCounter, 0)
        && writeRegister(RegisterFifoReadPointer, 0);
    // um bloco de média deixado por uma conexão anterior não continua este sinal
    m_decimator.reset();
    return isConfigured;
}

void Max30102::close()
{
    if(m_fileDescriptor < 0)
    {
        return;
    }
    // desliga os LEDs ao sair: o sensor fica aceso indefinidamente se o app morrer sem isso
    writeRegister(RegisterModeConfig, ModeShutdown);
    ::close(m_fileDescriptor);
    m_fileDescriptor = -1;
}

bool Max30102::readAvailableSamples(
    std::vector<PpgSample>& samples,
    std::vector<PpgSample>* fullRateSamples
)
{
    // FIFO_WR_PTR (0x04), OVF_COUNTER (0x05) e FIFO_RD_PTR (0x06) numa leitura só
    uint8_t pointers[3] = {};
    if(!readRegisters(RegisterFifoWritePointer, pointers, sizeof(pointers)))
    {
        return false;
    }
    const uint8_t writePointer = pointers[0];
    const uint8_t overflowCounter = pointers[1] & Max30102Fifo::PointerMask;
    const uint8_t readPointer = pointers[2];
    if(overflowCounter > 0)
    {
        m_lostSampleCount += overflowCounter;
    }
    const int availableCount =
        Max30102Fifo::availableSampleCount(writePointer, readPointer, overflowCounter);
    if(availableCount == 0)
    {
        return true;
    }

    uint8_t raw[Max30102Fifo::Depth * Max30102Fifo::BytesPerSample];
    if(!readRegisters(
           RegisterFifoData, raw, uint16_t(availableCount * Max30102Fifo::BytesPerSample)
       ))
    {
        return false;
    }
    for(int index = 0; index < availableCount; ++index)
    {
        const PpgSample sample =
            Max30102Fifo::decodeSample(&raw[index * Max30102Fifo::BytesPerSample]);
        if(fullRateSamples)
        {
            fullRateSamples->push_back(sample);
        }
        m_decimator.add(sample, samples);
    }
    return true;
}

bool Max30102::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    i2c_msg message = {SensorAddress, 0, sizeof(buffer), buffer};
    i2c_rdwr_ioctl_data transfer = {&message, 1};
    if(ioctl(m_fileDescriptor, I2C_RDWR, &transfer) < 0)
    {
        return fail(QStringLiteral("falha de I2C ao escrever 0x%1: %2")
                        .arg(reg, 2, 16, QLatin1Char('0')).arg(systemError()));
    }
    return true;
}

bool Max30102::readRegisters(uint8_t reg, uint8_t* destination, uint16_t length)
{
    i2c_msg messages[2] = {
        {SensorAddress, 0, 1, &reg},
        {SensorAddress, I2C_M_RD, length, destination}
    };
    i2c_rdwr_ioctl_data transfer = {messages, 2};
    if(ioctl(m_fileDescriptor, I2C_RDWR, &transfer) < 0)
    {
        return fail(QStringLiteral("falha de I2C ao ler 0x%1: %2")
                        .arg(reg, 2, 16, QLatin1Char('0')).arg(systemError()));
    }
    return true;
}

bool Max30102::fail(const QString& message)
{
    m_errorString = message;
    return false;
}
