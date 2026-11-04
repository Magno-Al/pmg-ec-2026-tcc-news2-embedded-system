#include "core/PpgProcessor.h"

#include <QTest>

#include <cmath>
#include <random>

namespace
{

// PPG sintético com parâmetros conhecidos. A respiração modula o sinal das três formas que o
// processador procura: linha de base (RIIV), amplitude do pulso (RIAV) e FC (arritmia sinusal
// respiratória, RIFV). A razão R entre as perfusões do vermelho e do IR define a SpO₂.
class SyntheticPpg
{
public:
    double heartRate = 72.0;
    double respiratoryRate = 15.0;
    double ratio = 0.6;
    double noiseCounts = 30.0;

    PpgSample next()
    {
        const double seconds = m_sampleIndex / PpgProcessor::SampleRateHz;
        ++m_sampleIndex;
        const double breath = std::sin(2.0 * M_PI * respiratoryRate / 60.0 * seconds);
        const double instantRate = heartRate * (1.0 + 0.04 * breath);
        m_cardiacPhase = std::fmod(m_cardiacPhase + instantRate / 60.0
                                   / PpgProcessor::SampleRateHz, 1.0);

        const double infraredDc = 120000.0 * (1.0 + 0.004 * breath);
        const double infraredPerfusion = 0.01 * (1.0 + 0.15 * breath);
        const double redDc = 140000.0 * (1.0 + 0.004 * breath);
        const double redPerfusion = ratio * infraredPerfusion;
        const double pulse = pulseShape(m_cardiacPhase);

        PpgSample sample;
        sample.infrared = uint32_t(infraredDc * (1.0 - infraredPerfusion * pulse) + noise());
        sample.red = uint32_t(redDc * (1.0 - redPerfusion * pulse) + noise());
        return sample;
    }

private:
    // subida rápida na sístole, descida lenta com a incisura dicrótica
    static double pulseShape(double phase)
    {
        const double main = phase < 0.15 ? phase / 0.15 : std::exp(-(phase - 0.15) * 4.0);
        const double notch = 0.2 * std::exp(-std::pow((phase - 0.45) / 0.05, 2));
        return main + notch;
    }

    double noise()
    {
        return std::normal_distribution<double>(0.0, noiseCounts)(m_generator);
    }

    int64_t m_sampleIndex = 0;
    double m_cardiacPhase = 0.0;
    std::mt19937 m_generator{7};
};

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
