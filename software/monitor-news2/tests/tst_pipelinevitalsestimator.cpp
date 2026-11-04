#include "SyntheticPpg.h"
#include "core/PipelineVitalsEstimator.h"

#include <QTest>

namespace
{

void feed(PipelineVitalsEstimator& estimator, SyntheticPpg& signal, double seconds)
{
    const int sampleCount = int(seconds * PpgProcessor::SampleRateHz);
    for(int index = 0; index < sampleCount; ++index)
    {
        estimator.addSample(signal.next());
    }
}

}

// O método em si é testado no ppg-pipeline; aqui, que o adaptador o alimenta como o main.cpp
// de lá (janela de 60 s, FC/SpO₂ dos últimos 30 s) com as amostras de 25 Hz do app.
class PipelineVitalsEstimatorTest : public QObject
{
    Q_OBJECT

private slots:
    void test_noReadingBeforeThirtySeconds();
    void test_cardiacReadingsAfterThirtySecondsRespiratoryOnlyAfterSixty();
    void test_heartRateMatchesPulse_data();
    void test_heartRateMatchesPulse();
    void test_oxygenSaturationFollowsLinearCalibration();
    void test_resetDiscardsTheCurrentWindow();
};

void PipelineVitalsEstimatorTest::test_noReadingBeforeThirtySeconds()
{
    PipelineVitalsEstimator estimator;
    SyntheticPpg signal;

    feed(estimator, signal, 29.0);

    QVERIFY(!estimator.heartRate());
    QVERIFY(!estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());
}

void PipelineVitalsEstimatorTest::test_cardiacReadingsAfterThirtySecondsRespiratoryOnlyAfterSixty()
{
    PipelineVitalsEstimator estimator;
    SyntheticPpg signal;

    // a janela 0–30 s carrega o transitório do filtro (o DC parte de zero) e é reprovada pelo
    // índice de perfusão; a de 5–35 s é a primeira com leitura
    feed(estimator, signal, 36.0);

    QVERIFY(estimator.heartRate());
    QVERIFY2(std::abs(*estimator.heartRate() - 72) <= 3,
             qPrintable(QString::number(*estimator.heartRate())));
    QVERIFY(estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());
}

void PipelineVitalsEstimatorTest::test_heartRateMatchesPulse_data()
{
    QTest::addColumn<double>("heartRate");
    QTest::newRow("bradicardia 50") << 50.0;
    QTest::newRow("repouso 72") << 72.0;
    QTest::newRow("taquicardia 115") << 115.0;
}

void PipelineVitalsEstimatorTest::test_heartRateMatchesPulse()
{
    QFETCH(double, heartRate);
    PipelineVitalsEstimator estimator;
    SyntheticPpg signal;
    signal.heartRate = heartRate;

    feed(estimator, signal, 65.0);

    QVERIFY(estimator.heartRate());
    QVERIFY2(std::abs(*estimator.heartRate() - heartRate) <= 3.0,
             qPrintable(QString::number(*estimator.heartRate())));
}

void PipelineVitalsEstimatorTest::test_oxygenSaturationFollowsLinearCalibration()
{
    // SpO₂ = 110 − 25·R do pipeline; R 0,6 → 95
    PipelineVitalsEstimator estimator;
    SyntheticPpg signal;
    signal.ratio = 0.6;

    feed(estimator, signal, 65.0);

    QVERIFY(estimator.oxygenSaturation());
    QVERIFY2(std::abs(*estimator.oxygenSaturation() - 95) <= 2,
             qPrintable(QString::number(*estimator.oxygenSaturation())));
}

void PipelineVitalsEstimatorTest::test_resetDiscardsTheCurrentWindow()
{
    PipelineVitalsEstimator estimator;
    SyntheticPpg signal;
    feed(estimator, signal, 65.0);
    QVERIFY(estimator.heartRate());

    estimator.reset();
    feed(estimator, signal, 29.0);

    QVERIFY(!estimator.heartRate());
}

QTEST_APPLESS_MAIN(PipelineVitalsEstimatorTest)
#include "tst_pipelinevitalsestimator.moc"
