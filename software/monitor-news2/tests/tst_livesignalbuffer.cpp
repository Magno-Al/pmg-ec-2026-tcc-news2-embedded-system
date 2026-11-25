#include "core/LiveSignalBuffer.h"

#include <QTest>

#include <atomic>
#include <thread>

// A thread de aquisição escreve as curvas e a thread da interface as lê a cada quadro.
class LiveSignalBufferTest : public QObject
{
    Q_OBJECT

private slots:
    void test_withoutDataReadsTheMiddleLine();
    void test_interpolatesBetweenSamples();
    void test_readsOnlyTheSamplesStillInTheBuffer();
    void test_readerSeesWrittenValuesWhileAnotherThreadWrites();
};

void LiveSignalBufferTest::test_withoutDataReadsTheMiddleLine()
{
    LiveSignalBuffer buffer(8);

    QVERIFY(!buffer.hasData());
    QCOMPARE(buffer.valueAt(3.0), 0.5);
}

void LiveSignalBufferTest::test_interpolatesBetweenSamples()
{
    LiveSignalBuffer buffer(8);
    buffer.append(0.2);
    buffer.append(0.6);

    QCOMPARE(buffer.writtenCount(), 2);
    QCOMPARE(buffer.valueAt(0.0), 0.2);
    QCOMPARE(buffer.valueAt(0.5), 0.4);
    QCOMPARE(buffer.valueAt(1.0), 0.6);
}

void LiveSignalBufferTest::test_readsOnlyTheSamplesStillInTheBuffer()
{
    LiveSignalBuffer buffer(4);
    for(int index = 0; index < 10; ++index)
    {
        buffer.append(index / 10.0);
    }

    // restam as amostras 6 a 9; antes disso lê a mais antiga, depois disso a mais nova
    QCOMPARE(buffer.valueAt(0.0), 0.6);
    QCOMPARE(buffer.valueAt(7.0), 0.7);
    QCOMPARE(buffer.valueAt(50.0), 0.9);
}

void LiveSignalBufferTest::test_readerSeesWrittenValuesWhileAnotherThreadWrites()
{
    // cada amostra vale o próprio índice / total: uma leitura da amostra k só pode dar k / total
    // ou, se ela já saiu do buffer, uma amostra mais nova
    constexpr int SampleCount = 200000;
    LiveSignalBuffer buffer(64);
    std::atomic<bool> hasFinishedWriting = false;
    std::thread writer([&buffer, &hasFinishedWriting]() {
        for(int index = 0; index < SampleCount; ++index)
        {
            buffer.append(double(index) / SampleCount);
        }
        hasFinishedWriting = true;
    });

    int inconsistentReadCount = 0;
    while(!hasFinishedWriting)
    {
        const int64_t newest = buffer.writtenCount() - 1;
        if(newest < 0)
        {
            continue;
        }
        const double value = buffer.valueAt(double(newest));
        const double expected = double(newest) / SampleCount;
        if(value < expected || value > 1.0)
        {
            ++inconsistentReadCount;
        }
    }
    writer.join();

    QCOMPARE(inconsistentReadCount, 0);
    QCOMPARE(buffer.writtenCount(), SampleCount);
    QCOMPARE(buffer.valueAt(SampleCount - 1), double(SampleCount - 1) / SampleCount);
}

QTEST_APPLESS_MAIN(LiveSignalBufferTest)
#include "tst_livesignalbuffer.moc"
