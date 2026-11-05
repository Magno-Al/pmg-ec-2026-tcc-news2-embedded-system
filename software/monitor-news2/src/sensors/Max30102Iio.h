#pragma once

#include "core/PpgProcessor.h"

#include <QString>

#include <cstdint>
#include <vector>

// MAX30102 pelo driver IIO do kernel (drivers/iio/health/max30102.c, módulo max30102 +
// overlay i2c-sensor,max30102,int_pin=17). O driver atende a interrupção do sensor, esvazia
// a FIFO do chip e põe as amostras num buffer do kernel, lido em /dev/iio:deviceN. A
// configuração do chip é a do driver, fixa no código dele: 400 Hz com média de 4 = 100
// amostras/s, ADC 4096 nA, pulso de 411 µs, LEDs a 7 mA (padrão do device tree). Para o
// PpgProcessor, cada 4 amostras viram uma, voltando aos mesmos 25 Hz do acesso por i2c-dev.
class Max30102Iio
{
public:
    Max30102Iio();
    ~Max30102Iio();

    Max30102Iio(const Max30102Iio&) = delete;
    Max30102Iio& operator=(const Max30102Iio&) = delete;

    // acha o iio:deviceN do max30102, liga os canais vermelho e IR e o buffer
    bool open();
    void close();
    bool isOpen() const { return m_fileDescriptor >= 0; }
    QString errorString() const { return m_errorString; }

    // o descritor fica legível quando o kernel tem amostras: dá para esperar por evento em
    // vez de consultar periodicamente
    int notificationDescriptor() const { return m_fileDescriptor; }

    // samples: 25 Hz (média de 4), como o PpgProcessor espera; fullRateSamples, se dado,
    // recebe também as amostras de 100 Hz do driver, sem a média
    bool readAvailableSamples(
        std::vector<PpgSample>& samples,
        std::vector<PpgSample>* fullRateSamples = nullptr
    );

    static constexpr int FullRateHz = 100;

private:
    struct ChannelFormat
    {
        int index = 0;
        int shift = 0;
        uint32_t mask = 0;
    };

    bool readChannelFormat(const QString& channel, ChannelFormat& format);
    bool writeAttribute(const QString& relativePath, const QByteArray& value);
    bool fail(const QString& message);

    QString m_devicePath;
    int m_fileDescriptor = -1;
    ChannelFormat m_redFormat;
    ChannelFormat m_infraredFormat;
    uint64_t m_redAccumulator = 0;
    uint64_t m_infraredAccumulator = 0;
    int m_accumulatedCount = 0;
    QString m_errorString;
};
