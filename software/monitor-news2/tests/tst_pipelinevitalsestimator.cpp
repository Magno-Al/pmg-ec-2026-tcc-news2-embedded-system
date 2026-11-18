#include "SyntheticPpg.h"
#include "core/PipelineVitalsEstimator.h"
#include "sensors/Max30102.h"

#include <QTest>

namespace
{

// a taxa em que o app alimenta o estimador
constexpr int SampleRateHz = Max30102::FullRateHz;

void feed(PipelineVitalsEstimator& estimator, SyntheticPpg& signal, double seconds)
{
    const int sampleCount = int(seconds * signal.sampleRateHz);
    for(int index = 0; index < sampleCount; ++index)
    {
        estimator.addSample(signal.next());
    }
}

// sinal sem pulso (dedo parado sem perfusão, por exemplo): o índice de perfusão fica abaixo de
// 0,4% e o pipeline reprova a janela
void feedWithoutPulse(PipelineVitalsEstimator& estimator, double seconds)
{
    const int sampleCount = int(seconds * SampleRateHz);
    for(int index = 0; index < sampleCount; ++index)
    {
        estimator.addSample(PPGSample{140000, 120000});
    }
}

}

// O método em si é testado no ppg-pipeline; aqui, que o adaptador o alimenta como o main.cpp
// de lá (janela de 60 s, FC/SpO₂ dos últimos 30 s) com as amostras de 100 Hz do app.
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
    void test_parametersAbsentUntilTheirFirstWindow();
    void test_rejectedWindowLeavesParametersAbsent();
    void test_returnsTheFilteredInfrared();
    void test_heartRateOnlyAtOneHundredHertz();
};

void PipelineVitalsEstimatorTest::test_noReadingBeforeThirtySeconds()
{
    PipelineVitalsEstimator estimator(SampleRateHz);
    SyntheticPpg signal;

    feed(estimator, signal, 29.0);

    QVERIFY(!estimator.heartRate());
    QVERIFY(!estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());
}

void PipelineVitalsEstimatorTest::test_cardiacReadingsAfterThirtySecondsRespiratoryOnlyAfterSixty()
{
    PipelineVitalsEstimator estimator(SampleRateHz);
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
    PipelineVitalsEstimator estimator(SampleRateHz);
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
    PipelineVitalsEstimator estimator(SampleRateHz);
    SyntheticPpg signal;
    signal.ratio = 0.6;

    feed(estimator, signal, 65.0);

    QVERIFY(estimator.oxygenSaturation());
    QVERIFY2(std::abs(*estimator.oxygenSaturation() - 95) <= 2,
             qPrintable(QString::number(*estimator.oxygenSaturation())));
}

void PipelineVitalsEstimatorTest::test_resetDiscardsTheCurrentWindow()
{
    PipelineVitalsEstimator estimator(SampleRateHz);
    SyntheticPpg signal;
    feed(estimator, signal, 65.0);
    QVERIFY(estimator.heartRate());

    estimator.reset();
    feed(estimator, signal, 29.0);

    QVERIFY(!estimator.heartRate());
}

void PipelineVitalsEstimatorTest::test_parametersAbsentUntilTheirFirstWindow()
{
    // ausência no início: nenhum valor antes da primeira janela de 30 s, e a FR só com a de
    // 60 s, mesmo com FC e SpO₂ já presentes
    PipelineVitalsEstimator estimator(SampleRateHz);
    SyntheticPpg signal;

    feed(estimator, signal, 29.9);
    QVERIFY(!estimator.heartRate());
    QVERIFY(!estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());

    feed(estimator, signal, 30.0);
    QVERIFY(estimator.heartRate());
    QVERIFY(estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());

    // a janela de 0–60 s carrega o transitório do filtro; a de 5–65 s é a primeira com FR
    feed(estimator, signal, 5.2);
    QVERIFY(estimator.respiratoryRate());
}

void PipelineVitalsEstimatorTest::test_rejectedWindowLeavesParametersAbsent()
{
    // a janela reprovada deixa o parâmetro ausente: o valor da janela anterior não se repete
    PipelineVitalsEstimator estimator(SampleRateHz);
    SyntheticPpg signal;
    feed(estimator, signal, 65.0);
    QVERIFY(estimator.heartRate());
    QVERIFY(estimator.oxygenSaturation());
    QVERIFY(estimator.respiratoryRate());

    // janela de 30 s toda sem pulso: FC e SpO₂ reprovadas
    feedWithoutPulse(estimator, 35.0);
    QVERIFY(!estimator.heartRate());
    QVERIFY(!estimator.preciseHeartRate());
    QVERIFY(!estimator.oxygenSaturation());

    // janela de 60 s toda sem pulso: FR reprovada
    feedWithoutPulse(estimator, 30.0);
    QVERIFY(!estimator.respiratoryRate());

    // o pulso de volta traz os valores de volta, sem reset
    feed(estimator, signal, 35.0);
    QVERIFY(estimator.heartRate());
}

void PipelineVitalsEstimatorTest::test_returnsTheFilteredInfrared()
{
    // a curva da tela usa o IR filtrado pelo PPGBandpassFilter do pipeline, o mesmo da janela
    PipelineVitalsEstimator estimator(SampleRateHz);
    PPGBandpassFilter reference(PPG_FILTER_LOW_CUTOFF_HZ, PPG_FILTER_HIGH_CUTOFF_HZ, SampleRateHz);
    SyntheticPpg signal;
    for(int index = 0; index < 10 * SampleRateHz; ++index)
    {
        const PPGSample sample = signal.next();
        QCOMPARE(estimator.addSample(sample), reference.process(sample.ir));
    }
}

void PipelineVitalsEstimatorTest::test_heartRateOnlyAtOneHundredHertz()
{
    // só FC: não é usado pelo app; o pipeline-window-benchmark mede com ele a alternativa de FC
    // a 100 Hz e SpO₂/FR a 25 Hz
    PipelineVitalsEstimator estimator(100, PipelineVitalsEstimator::Outputs::HeartRateOnly);
    SyntheticPpg signal;
    signal.sampleRateHz = 100.0;
    signal.heartRate = 73.0;

    feed(estimator, signal, 40.0);

    QVERIFY(estimator.preciseHeartRate());
    QVERIFY2(std::abs(*estimator.preciseHeartRate() - 73.0) <= 1.0,
             qPrintable(QString::number(*estimator.preciseHeartRate())));
    QVERIFY(!estimator.oxygenSaturation());
    QVERIFY(!estimator.respiratoryRate());
}

QTEST_APPLESS_MAIN(PipelineVitalsEstimatorTest)
#include "tst_pipelinevitalsestimator.moc"
