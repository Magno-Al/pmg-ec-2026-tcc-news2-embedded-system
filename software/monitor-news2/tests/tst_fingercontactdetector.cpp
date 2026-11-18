#include "core/FingerContactDetector.h"

#include <QTest>

namespace
{

constexpr int SampleRateHz = 100;

// quantas amostras de IR iguais até o detector mudar de estado (0 se não mudar)
int samplesUntilChange(FingerContactDetector& detector, uint32_t infrared, int maximum)
{
    for(int count = 1; count <= maximum; ++count)
    {
        if(detector.update(infrared))
        {
            return count;
        }
    }
    return 0;
}

}

class FingerContactDetectorTest : public QObject
{
    Q_OBJECT

private slots:
    void test_startsWithoutContact();
    void test_contactConfirmedAfterTheConfirmationTime();
    void test_briefReflectionDoesNotCountAsFinger();
    void test_lossConfirmedAfterTheConfirmationTime();
    void test_hysteresisKeepsTheCurrentState();
    void test_resetDropsTheContact();
};

void FingerContactDetectorTest::test_startsWithoutContact()
{
    FingerContactDetector detector(SampleRateHz);

    QCOMPARE(samplesUntilChange(detector, 1000, 10 * SampleRateHz), 0);
    QVERIFY(!detector.hasFingerContact());
}

void FingerContactDetectorTest::test_contactConfirmedAfterTheConfirmationTime()
{
    // 0,48 s a 100 Hz = 48 amostras seguidas acima de 50.000
    FingerContactDetector detector(SampleRateHz);

    QCOMPARE(samplesUntilChange(detector, 120000, 10 * SampleRateHz), 48);
    QVERIFY(detector.hasFingerContact());
}

void FingerContactDetectorTest::test_briefReflectionDoesNotCountAsFinger()
{
    FingerContactDetector detector(SampleRateHz);

    QCOMPARE(samplesUntilChange(detector, 120000, 47), 0);
    QCOMPARE(samplesUntilChange(detector, 1000, 1), 0);
    // a contagem recomeça depois da amostra abaixo do limiar
    QCOMPARE(samplesUntilChange(detector, 120000, 47), 0);
    QVERIFY(!detector.hasFingerContact());
}

void FingerContactDetectorTest::test_lossConfirmedAfterTheConfirmationTime()
{
    // 0,32 s a 100 Hz = 32 amostras seguidas abaixo de 30.000
    FingerContactDetector detector(SampleRateHz);
    samplesUntilChange(detector, 120000, 48);

    QCOMPARE(samplesUntilChange(detector, 1000, 10 * SampleRateHz), 32);
    QVERIFY(!detector.hasFingerContact());
}

void FingerContactDetectorTest::test_hysteresisKeepsTheCurrentState()
{
    // entre 30.000 e 50.000 o estado não muda, nos dois sentidos
    FingerContactDetector detector(SampleRateHz);
    QCOMPARE(samplesUntilChange(detector, 40000, 10 * SampleRateHz), 0);
    QVERIFY(!detector.hasFingerContact());

    samplesUntilChange(detector, 120000, 48);
    QCOMPARE(samplesUntilChange(detector, 40000, 10 * SampleRateHz), 0);
    QVERIFY(detector.hasFingerContact());
}

void FingerContactDetectorTest::test_resetDropsTheContact()
{
    FingerContactDetector detector(SampleRateHz);
    samplesUntilChange(detector, 120000, 48);

    detector.reset();

    QVERIFY(!detector.hasFingerContact());
    QCOMPARE(samplesUntilChange(detector, 120000, 10 * SampleRateHz), 48);
}

QTEST_APPLESS_MAIN(FingerContactDetectorTest)
#include "tst_fingercontactdetector.moc"
