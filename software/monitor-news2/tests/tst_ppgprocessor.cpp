#include "SyntheticPpg.h"
#include "core/PpgProcessor.h"

#include <QTest>

namespace
{

void feed(PpgProcessor& processor, SyntheticPpg& signal, double seconds)
{
    const int sampleCount = int(seconds * PpgProcessor::SampleRateHz);
    for(int index = 0; index < sampleCount; ++index)
    {
        processor.addSample(signal.next());
    }
}

void feedWithoutFinger(PpgProcessor& processor, double seconds)
{
    const int sampleCount = int(seconds * PpgProcessor::SampleRateHz);
    for(int index = 0; index < sampleCount; ++index)
    {
        processor.addSample({900, 700});
    }
}

}

class PpgProcessorTest : public QObject
{
    Q_OBJECT

private slots:
    void test_withoutFingerReportsNothing();
    void test_briefReflectionDoesNotCountAsFinger();
    void test_heartRateMatchesPulse_data();
    void test_heartRateMatchesPulse();
    void test_heartRateAppearsWithinTenSeconds();
    void test_oxygenSaturationFollowsRatio_data();
    void test_oxygenSaturationFollowsRatio();
    void test_respiratoryRateMatchesBreathing_data();
    void test_respiratoryRateMatchesBreathing();
    void test_respiratoryRateWaitsForFullWindow();
    void test_removingFingerClearsMeasurements();
    void test_displaySamplesStayNormalized();
};

void PpgProcessorTest::test_withoutFingerReportsNothing()
{
    PpgProcessor processor;
    feedWithoutFinger(processor, 10.0);

    QVERIFY(!processor.hasFingerContact());
    QVERIFY(!processor.heartRate());
    QVERIFY(!processor.oxygenSaturation());
    QVERIFY(!processor.respiratoryRate());
}

void PpgProcessorTest::test_briefReflectionDoesNotCountAsFinger()
{
    PpgProcessor processor;
    for(int index = 0; index < 5; ++index)
    {
        processor.addSample({90000, 80000});
    }
    feedWithoutFinger(processor, 1.0);

    QVERIFY(!processor.hasFingerContact());
}

void PpgProcessorTest::test_heartRateMatchesPulse_data()
{
    QTest::addColumn<double>("heartRate");
    QTest::newRow("bradicardia 45") << 45.0;
    QTest::newRow("repouso 72") << 72.0;
    QTest::newRow("taquicardia 115") << 115.0;
    QTest::newRow("taquicardia 140") << 140.0;
}

void PpgProcessorTest::test_heartRateMatchesPulse()
{
    QFETCH(double, heartRate);
    PpgProcessor processor;
    SyntheticPpg signal;
    signal.heartRate = heartRate;

    feed(processor, signal, 20.0);

    QVERIFY(processor.hasFingerContact());
    QVERIFY(processor.heartRate());
    QVERIFY2(std::abs(*processor.heartRate() - heartRate) <= 2.0,
             qPrintable(QString::number(*processor.heartRate())));
}

void PpgProcessorTest::test_heartRateAppearsWithinTenSeconds()
{
    PpgProcessor processor;
    SyntheticPpg signal;
    feedWithoutFinger(processor, 2.0);

    feed(processor, signal, 10.0);

    QVERIFY(processor.heartRate());
}

void PpgProcessorTest::test_oxygenSaturationFollowsRatio_data()
{
    QTest::addColumn<double>("ratio");
    QTest::addColumn<int>("expectedSaturation");
    // SpO₂ = -45,060·R² + 30,354·R + 94,845 (Maxim RD117)
    QTest::newRow("R 0,5 -> 99") << 0.5 << 99;
    QTest::newRow("R 0,7 -> 94") << 0.7 << 94;
    QTest::newRow("R 0,9 -> 86") << 0.9 << 86;
}

void PpgProcessorTest::test_oxygenSaturationFollowsRatio()
{
    QFETCH(double, ratio);
    QFETCH(int, expectedSaturation);
    PpgProcessor processor;
    SyntheticPpg signal;
    signal.ratio = ratio;

    feed(processor, signal, 20.0);

    QVERIFY(processor.oxygenSaturation());
    QVERIFY2(std::abs(*processor.oxygenSaturation() - expectedSaturation) <= 1,
             qPrintable(QString::number(*processor.oxygenSaturation())));
}

void PpgProcessorTest::test_respiratoryRateMatchesBreathing_data()
{
    QTest::addColumn<double>("respiratoryRate");
    QTest::newRow("bradipneia 8") << 8.0;
    QTest::newRow("normal 14") << 14.0;
    QTest::newRow("NEWS2 3 pontos 22") << 22.0;
    QTest::newRow("taquipneia 27 (2,7 batimentos por respiracao)") << 27.0;
    QTest::newRow("taquipneia 32") << 32.0;
}

void PpgProcessorTest::test_respiratoryRateMatchesBreathing()
{
    QFETCH(double, respiratoryRate);
    PpgProcessor processor;
    SyntheticPpg signal;
    signal.respiratoryRate = respiratoryRate;

    feed(processor, signal, 50.0);

    QVERIFY(processor.respiratoryRate());
    QVERIFY2(std::abs(*processor.respiratoryRate() - respiratoryRate) <= 1.5,
             qPrintable(QString::number(*processor.respiratoryRate())));
}

void PpgProcessorTest::test_respiratoryRateWaitsForFullWindow()
{
    PpgProcessor processor;
    SyntheticPpg signal;

    feed(processor, signal, 25.0);

    QVERIFY(processor.heartRate());
    QVERIFY(!processor.respiratoryRate());
}

void PpgProcessorTest::test_removingFingerClearsMeasurements()
{
    PpgProcessor processor;
    SyntheticPpg signal;
    feed(processor, signal, 50.0);
    QVERIFY(processor.heartRate());
    QVERIFY(processor.respiratoryRate());

    feedWithoutFinger(processor, 1.0);

    QVERIFY(!processor.hasFingerContact());
    QVERIFY(!processor.heartRate());
    QVERIFY(!processor.oxygenSaturation());
    QVERIFY(!processor.respiratoryRate());
}

void PpgProcessorTest::test_displaySamplesStayNormalized()
{
    PpgProcessor processor;
    SyntheticPpg signal;
    int plethysmographyCount = 0;
    int respirationCount = 0;
    for(int index = 0; index < 500; ++index)
    {
        const PpgProcessor::Output output = processor.addSample(signal.next());
        if(output.plethysmographyDisplay)
        {
            ++plethysmographyCount;
            QVERIFY(*output.plethysmographyDisplay >= 0.0);
            QVERIFY(*output.plethysmographyDisplay <= 1.0);
        }
        for(double respirationValue : output.respirationDisplay)
        {
            ++respirationCount;
            QVERIFY(respirationValue >= 0.0);
            QVERIFY(respirationValue <= 1.0);
        }
    }
    QVERIFY(plethysmographyCount > 400);
    QVERIFY(respirationCount > 40);
}

QTEST_APPLESS_MAIN(PpgProcessorTest)
#include "tst_ppgprocessor.moc"
