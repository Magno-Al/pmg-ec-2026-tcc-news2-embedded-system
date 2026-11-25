#include "sensors/Max30102Fifo.h"

#include <QTest>

class Max30102FifoTest : public QObject
{
    Q_OBJECT

private slots:
    void test_countsSamplesBetweenPointersWithoutWrap();
    void test_countsSamplesWhenWritePointerWrapsFrom31To0();
    void test_equalPointersWithoutOverflowAreAnEmptyFifo();
    void test_equalPointersWithOverflowAreAFullFifo();
    void test_overflowMeansAFullFifoEvenWhenPointersDiffer();
    void test_ignoresBitsAboveThePointerWidth();

    void test_assemblesChannelMostSignificantByteFirst();
    void test_discardsBitsAboveThe18thBit();
    void test_decodesRedThenInfrared();
};

void Max30102FifoTest::test_countsSamplesBetweenPointersWithoutWrap()
{
    QCOMPARE(Max30102Fifo::availableSampleCount(9, 5, 0), 4);
    QCOMPARE(Max30102Fifo::availableSampleCount(31, 0, 0), 31);
}

void Max30102FifoTest::test_countsSamplesWhenWritePointerWrapsFrom31To0()
{
    // posições 30, 31, 0 e 1
    QCOMPARE(Max30102Fifo::availableSampleCount(2, 30, 0), 4);
    // só a 31
    QCOMPARE(Max30102Fifo::availableSampleCount(0, 31, 0), 1);
}

void Max30102FifoTest::test_equalPointersWithoutOverflowAreAnEmptyFifo()
{
    QCOMPARE(Max30102Fifo::availableSampleCount(0, 0, 0), 0);
    QCOMPARE(Max30102Fifo::availableSampleCount(17, 17, 0), 0);
}

void Max30102FifoTest::test_equalPointersWithOverflowAreAFullFifo()
{
    QCOMPARE(Max30102Fifo::availableSampleCount(17, 17, 1), Max30102Fifo::Depth);
    // o contador satura em 0x1F
    QCOMPARE(Max30102Fifo::availableSampleCount(0, 0, 0x1F), Max30102Fifo::Depth);
}

void Max30102FifoTest::test_overflowMeansAFullFifoEvenWhenPointersDiffer()
{
    // Visto no equipamento depois de a leitura atrasar mais de 320 ms: FIFO_RD_PTR 0x16,
    // FIFO_WR_PTR 0x18, OVF_COUNTER 3. A FIFO está cheia e os ponteiros já deram a volta;
    // ler só as 2 amostras da diferença deixava a FIFO cheia para sempre.
    QCOMPARE(Max30102Fifo::availableSampleCount(0x18, 0x16, 3), Max30102Fifo::Depth);
    QCOMPARE(Max30102Fifo::availableSampleCount(9, 5, 1), Max30102Fifo::Depth);
}

void Max30102FifoTest::test_ignoresBitsAboveThePointerWidth()
{
    // bits 7–5 dos registradores são reservados; ligados, não podem mudar a conta
    QCOMPARE(Max30102Fifo::availableSampleCount(0xE9, 0x05, 0), 4);
    QCOMPARE(Max30102Fifo::availableSampleCount(0x05, 0x05, 0xE0), 0);
}

void Max30102FifoTest::test_assemblesChannelMostSignificantByteFirst()
{
    const uint8_t typicalInfrared[3] = {0x01, 0xE2, 0x40};
    QCOMPARE(Max30102Fifo::channelValue(typicalInfrared), uint32_t(123456));

    const uint8_t fullScale[3] = {0x03, 0xFF, 0xFF};
    QCOMPARE(Max30102Fifo::channelValue(fullScale), uint32_t(262143));

    const uint8_t zero[3] = {0x00, 0x00, 0x00};
    QCOMPARE(Max30102Fifo::channelValue(zero), uint32_t(0));
}

void Max30102FifoTest::test_discardsBitsAboveThe18thBit()
{
    // 0xFD E2 40: os bits 23–18 (0xFC no primeiro byte) não são do ADC; sobra 0x01E240
    const uint8_t withUpperBits[3] = {0xFD, 0xE2, 0x40};
    QCOMPARE(Max30102Fifo::channelValue(withUpperBits), uint32_t(123456));

    const uint8_t allOnes[3] = {0xFF, 0xFF, 0xFF};
    QCOMPARE(Max30102Fifo::channelValue(allOnes), Max30102Fifo::SampleMask);
}

void Max30102FifoTest::test_decodesRedThenInfrared()
{
    const uint8_t bytes[Max30102Fifo::BytesPerSample] = {0x02, 0x22, 0xE0, 0xFD, 0xE2, 0x40};

    const PpgSample sample = Max30102Fifo::decodeSample(bytes);

    QCOMPARE(sample.red, uint32_t(0x0222E0));
    QCOMPARE(sample.infrared, uint32_t(123456));
}

QTEST_APPLESS_MAIN(Max30102FifoTest)
#include "tst_max30102fifo.moc"
