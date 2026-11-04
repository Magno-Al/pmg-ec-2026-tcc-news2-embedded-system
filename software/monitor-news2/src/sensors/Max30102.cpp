#include "Max30102.h"

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
constexpr int FifoDepth = 32;
constexpr int BytesPerSample = 6;

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
// faixa do ADC 4096 nA, 100 amostras/s, pulso de 411 µs (18 bits)
constexpr uint8_t Spo2Range4096Rate100Pulse411 = 0x27;
// ~7 mA: com o dedo, IR na casa de 10⁵ contagens, longe da saturação (2¹⁸)
constexpr uint8_t LedAmplitude = 0x24;

QString systemError()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

}

Max30102::Max30102(QString devicePath)
    : m_devicePath(std::move(devicePath))
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
        && writeRegister(RegisterSpo2Config, Spo2Range4096Rate100Pulse411)
        && writeRegister(RegisterLedRedAmplitude, LedAmplitude)
        && writeRegister(RegisterLedInfraredAmplitude, LedAmplitude)
        && writeRegister(RegisterFifoWritePointer, 0)
        && writeRegister(RegisterFifoOverflowCounter, 0)
        && writeRegister(RegisterFifoReadPointer, 0);
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

bool Max30102::readAvailableSamples(std::vector<PpgSample>& samples)
{
    uint8_t pointers[3] = {};
    if(!readRegisters(RegisterFifoWritePointer, pointers, sizeof(pointers)))
    {
        return false;
    }
    const uint8_t writePointer = pointers[0];
    const uint8_t readPointer = pointers[2];
    const int availableCount = (writePointer - readPointer) & (FifoDepth - 1);
    if(availableCount == 0)
    {
        return true;
    }

    uint8_t raw[FifoDepth * BytesPerSample];
    if(!readRegisters(RegisterFifoData, raw, uint16_t(availableCount * BytesPerSample)))
    {
        return false;
    }
    for(int index = 0; index < availableCount; ++index)
    {
        const uint8_t* bytes = &raw[index * BytesPerSample];
        PpgSample sample;
        sample.red = ((uint32_t(bytes[0]) << 16) | (bytes[1] << 8) | bytes[2]) & 0x3FFFF;
        sample.infrared = ((uint32_t(bytes[3]) << 16) | (bytes[4] << 8) | bytes[5]) & 0x3FFFF;
        samples.push_back(sample);
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
