#include "Max30102Iio.h"

#include <fcntl.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <cerrno>
#include <cstring>

namespace
{

const QString IioDevicesPath = QStringLiteral("/sys/bus/iio/devices");
const QByteArray DriverName = QByteArrayLiteral("max30102");
// 100 amostras/s do driver -> 25 amostras/s do PpgProcessor
constexpr int DecimationFactor = Max30102Iio::FullRateHz / 25;
// 2,5 s a 100 Hz; o padrão do kernel é 2 amostras, que transbordaria num engasgo da interface
constexpr int KernelBufferLength = 256;
constexpr int BytesPerScan = 8;
constexpr int ScansPerRead = 64;

QString systemError()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

}

Max30102Iio::Max30102Iio() = default;

Max30102Iio::~Max30102Iio()
{
    close();
}

bool Max30102Iio::open()
{
    close();
    m_devicePath.clear();
    const QStringList devices = QDir(IioDevicesPath).entryList({"iio:device*"}, QDir::Dirs);
    for(const QString& device : devices)
    {
        QFile name(IioDevicesPath + '/' + device + "/name");
        if(name.open(QIODevice::ReadOnly) && name.readAll().trimmed() == DriverName)
        {
            m_devicePath = IioDevicesPath + '/' + device;
            break;
        }
    }
    if(m_devicePath.isEmpty())
    {
        return fail(QStringLiteral("driver IIO max30102 não carregado (modprobe max30102 e "
                                   "dtoverlay=i2c-sensor,max30102,int_pin=17)"));
    }

    // o buffer só aceita mudanças desligado; pode ter ficado ligado se o app morreu
    writeAttribute(QStringLiteral("buffer/enable"), "0");
    const bool isConfigured =
        writeAttribute(QStringLiteral("scan_elements/in_intensity_red_en"), "1")
        && writeAttribute(QStringLiteral("scan_elements/in_intensity_ir_en"), "1")
        && writeAttribute(QStringLiteral("buffer/length"), QByteArray::number(KernelBufferLength))
        && readChannelFormat(QStringLiteral("red"), m_redFormat)
        && readChannelFormat(QStringLiteral("ir"), m_infraredFormat)
        && writeAttribute(QStringLiteral("buffer/enable"), "1");
    if(!isConfigured)
    {
        return false;
    }

    const QString characterDevice = "/dev/" + QDir(m_devicePath).dirName();
    m_fileDescriptor = ::open(characterDevice.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if(m_fileDescriptor < 0)
    {
        writeAttribute(QStringLiteral("buffer/enable"), "0");
        return fail(QStringLiteral("não abriu %1: %2").arg(characterDevice, systemError()));
    }
    m_redAccumulator = 0;
    m_infraredAccumulator = 0;
    m_accumulatedCount = 0;
    return true;
}

void Max30102Iio::close()
{
    if(m_fileDescriptor < 0)
    {
        return;
    }
    ::close(m_fileDescriptor);
    m_fileDescriptor = -1;
    // desligar o buffer põe o sensor em shutdown (LEDs apagados), pelo próprio driver
    writeAttribute(QStringLiteral("buffer/enable"), "0");
}

bool Max30102Iio::readAvailableSamples(
    std::vector<PpgSample>& samples,
    std::vector<PpgSample>* fullRateSamples
)
{
    uint8_t raw[ScansPerRead * BytesPerScan];
    while(true)
    {
        const ssize_t byteCount = ::read(m_fileDescriptor, raw, sizeof(raw));
        if(byteCount < 0)
        {
            if(errno == EAGAIN)
            {
                return true;
            }
            return fail(QStringLiteral("falha ao ler o buffer IIO: %1").arg(systemError()));
        }
        if(byteCount == 0)
        {
            return true;
        }
        const int scanCount = int(byteCount) / BytesPerScan;
        for(int scan = 0; scan < scanCount; ++scan)
        {
            const uint8_t* bytes = &raw[scan * BytesPerScan];
            const auto channelValue = [bytes](const ChannelFormat& format) {
                const uint8_t* word = bytes + format.index * 4;
                const uint32_t bigEndian = (uint32_t(word[0]) << 24) | (uint32_t(word[1]) << 16)
                    | (uint32_t(word[2]) << 8) | word[3];
                return (bigEndian >> format.shift) & format.mask;
            };
            const uint32_t red = channelValue(m_redFormat);
            const uint32_t infrared = channelValue(m_infraredFormat);
            if(fullRateSamples)
            {
                fullRateSamples->push_back({red, infrared});
            }
            m_redAccumulator += red;
            m_infraredAccumulator += infrared;
            ++m_accumulatedCount;
            if(m_accumulatedCount == DecimationFactor)
            {
                PpgSample sample;
                sample.red = uint32_t(m_redAccumulator / DecimationFactor);
                sample.infrared = uint32_t(m_infraredAccumulator / DecimationFactor);
                samples.push_back(sample);
                m_redAccumulator = 0;
                m_infraredAccumulator = 0;
                m_accumulatedCount = 0;
            }
        }
        if(byteCount < ssize_t(sizeof(raw)))
        {
            return true;
        }
    }
}

bool Max30102Iio::readChannelFormat(const QString& channel, ChannelFormat& format)
{
    const QString prefix = m_devicePath + "/scan_elements/in_intensity_" + channel;
    QFile index(prefix + "_index");
    QFile type(prefix + "_type");
    if(!index.open(QIODevice::ReadOnly) || !type.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("canal %1 ausente em %2").arg(channel, m_devicePath));
    }
    // ex.: "be:u18/32>>8" — big-endian, 18 bits úteis em 32, deslocados 8
    static const QRegularExpression pattern(QStringLiteral("^be:u(\\d+)/32>>(\\d+)$"));
    const QString typeText = QString::fromLatin1(type.readAll().trimmed());
    const QRegularExpressionMatch match = pattern.match(typeText);
    if(!match.hasMatch())
    {
        return fail(QStringLiteral("formato inesperado do canal %1: %2").arg(channel, typeText));
    }
    // o layout lido é de dois canais de 32 bits, então o índice tem que ser 0 ou 1
    format.index = index.readAll().trimmed().toInt();
    if(format.index < 0 || format.index > 1)
    {
        return fail(QStringLiteral("índice inesperado do canal %1").arg(channel));
    }
    format.shift = match.captured(2).toInt();
    format.mask = (1u << match.captured(1).toInt()) - 1u;
    return true;
}

bool Max30102Iio::writeAttribute(const QString& relativePath, const QByteArray& value)
{
    QFile attribute(m_devicePath + '/' + relativePath);
    if(!attribute.open(QIODevice::WriteOnly) || attribute.write(value) != value.size())
    {
        return fail(QStringLiteral("não escreveu %1 em %2: %3")
                        .arg(QString::fromLatin1(value), relativePath, attribute.errorString()));
    }
    return true;
}

bool Max30102Iio::fail(const QString& message)
{
    m_errorString = message;
    return false;
}
