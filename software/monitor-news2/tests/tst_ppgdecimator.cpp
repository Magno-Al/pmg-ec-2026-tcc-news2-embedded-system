#include "core/PpgDecimator.h"

#include <QTest>

#include <vector>

namespace
{

// sequência conhecida: a amostra i tem vermelho 1000 + i e IR 50000 + 10·i
PpgSample knownSample(int index)
{
    return PpgSample{uint32_t(1000 + index), uint32_t(50000 + 10 * index)};
}

// média esperada do bloco b de 4 amostras consecutivas da sequência conhecida
PpgSample expectedBlockAverage(int block)
{
    uint64_t redSum = 0;
    uint64_t infraredSum = 0;
    for(int index = 4 * block; index < 4 * block + 4; ++index)
    {
        redSum += knownSample(index).red;
        infraredSum += knownSample(index).infrared;
    }
    return PpgSample{uint32_t(redSum / 4), uint32_t(infraredSum / 4)};
}

// entrega a sequência conhecida em leituras com as quantidades dadas, como o sensor faria
std::vector<PpgSample> decimateInReads(PpgDecimator& decimator, const std::vector<int>& reads)
{
    std::vector<PpgSample> output;
    int index = 0;
    for(int count : reads)
    {
        for(int i = 0; i < count; ++i)
        {
            decimator.add(knownSample(index++), output);
        }
    }
    return output;
}

}

class PpgDecimatorTest : public QObject
{
    Q_OBJECT

private slots:
    void test_averagesEachBlockOfFour();
    void test_readsMultipleOfFourGiveOneOutputPerFourInputs();
    void test_keepsTheIncompleteBlockForTheNextRead();
    void test_noSampleIsLostAcrossReadsThatAreNotMultiplesOfFour();
    void test_averageTruncatesLikeIntegerDivision();
    void test_resetDiscardsTheIncompleteBlock();
    void test_sumsDoNotOverflowAtFullScale();
};

void PpgDecimatorTest::test_averagesEachBlockOfFour()
{
    PpgDecimator decimator(4);
    std::vector<PpgSample> output;

    decimator.add({100, 200}, output);
    decimator.add({104, 208}, output);
    decimator.add({108, 216}, output);
    QVERIFY(output.empty());
    decimator.add({112, 224}, output);

    QCOMPARE(output.size(), size_t(1));
    QCOMPARE(output[0].red, uint32_t(106));
    QCOMPARE(output[0].infrared, uint32_t(212));
    QCOMPARE(decimator.pendingCount(), 0);
}

void PpgDecimatorTest::test_readsMultipleOfFourGiveOneOutputPerFourInputs()
{
    PpgDecimator decimator(4);

    const std::vector<PpgSample> output = decimateInReads(decimator, {4, 8, 4, 12});

    QCOMPARE(output.size(), size_t(7));
    for(int block = 0; block < 7; ++block)
    {
        QCOMPARE(output[size_t(block)].red, expectedBlockAverage(block).red);
        QCOMPARE(output[size_t(block)].infrared, expectedBlockAverage(block).infrared);
    }
    QCOMPARE(decimator.pendingCount(), 0);
}

void PpgDecimatorTest::test_keepsTheIncompleteBlockForTheNextRead()
{
    PpgDecimator decimator(4);
    std::vector<PpgSample> output;

    // primeira leitura: 6 amostras -> 1 bloco, sobram 2
    for(int index = 0; index < 6; ++index)
    {
        decimator.add(knownSample(index), output);
    }
    QCOMPARE(output.size(), size_t(1));
    QCOMPARE(decimator.pendingCount(), 2);

    // segunda leitura: 2 amostras completam o bloco das 2 que sobraram
    for(int index = 6; index < 8; ++index)
    {
        decimator.add(knownSample(index), output);
    }
    QCOMPARE(output.size(), size_t(2));
    QCOMPARE(output[1].red, expectedBlockAverage(1).red);
    QCOMPARE(output[1].infrared, expectedBlockAverage(1).infrared);
    QCOMPARE(decimator.pendingCount(), 0);
}

void PpgDecimatorTest::test_noSampleIsLostAcrossReadsThatAreNotMultiplesOfFour()
{
    PpgDecimator decimator(4);
    // leituras irregulares, como no polling de 40 ms a 100 Hz: 103 amostras no total
    const std::vector<int> reads = {3, 5, 4, 1, 7, 2, 9, 4, 6, 3, 30, 1, 28};
    int total = 0;
    for(int count : reads)
    {
        total += count;
    }

    const std::vector<PpgSample> output = decimateInReads(decimator, reads);

    // todas as amostras viram médias, a não ser as do último bloco ainda incompleto
    QCOMPARE(int(output.size()) * 4 + decimator.pendingCount(), total);
    QCOMPARE(output.size(), size_t(total / 4));
    QCOMPARE(decimator.pendingCount(), total % 4);
    // e cada média é a de 4 amostras consecutivas da sequência, sem pular nem repetir nenhuma
    for(int block = 0; block < int(output.size()); ++block)
    {
        QCOMPARE(output[size_t(block)].red, expectedBlockAverage(block).red);
        QCOMPARE(output[size_t(block)].infrared, expectedBlockAverage(block).infrared);
    }
}

void PpgDecimatorTest::test_averageTruncatesLikeIntegerDivision()
{
    PpgDecimator decimator(4);
    std::vector<PpgSample> output;

    // soma 7 -> 1,75 -> 1; soma 11 -> 2,75 -> 2
    decimator.add({1, 2}, output);
    decimator.add({2, 3}, output);
    decimator.add({2, 3}, output);
    decimator.add({2, 3}, output);

    QCOMPARE(output.size(), size_t(1));
    QCOMPARE(output[0].red, uint32_t(1));
    QCOMPARE(output[0].infrared, uint32_t(2));
}

void PpgDecimatorTest::test_resetDiscardsTheIncompleteBlock()
{
    PpgDecimator decimator(4);
    std::vector<PpgSample> output;
    decimator.add({999, 999}, output);
    decimator.add({999, 999}, output);

    decimator.reset();
    for(int i = 0; i < 4; ++i)
    {
        decimator.add({10, 20}, output);
    }

    QCOMPARE(output.size(), size_t(1));
    QCOMPARE(output[0].red, uint32_t(10));
    QCOMPARE(output[0].infrared, uint32_t(20));
}

void PpgDecimatorTest::test_sumsDoNotOverflowAtFullScale()
{
    PpgDecimator decimator(4);
    std::vector<PpgSample> output;
    for(int i = 0; i < 4; ++i)
    {
        decimator.add({UINT32_MAX, 262143}, output);
    }

    QCOMPARE(output.size(), size_t(1));
    QCOMPARE(output[0].red, UINT32_MAX);
    QCOMPARE(output[0].infrared, uint32_t(262143));
}

QTEST_APPLESS_MAIN(PpgDecimatorTest)
#include "tst_ppgdecimator.moc"
