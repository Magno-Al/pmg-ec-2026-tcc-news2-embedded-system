#include "core/News2Score.h"

#include <QTest>

using News2::Parameter;
using News2::ResponseLevel;

class News2ScoreTest : public QObject
{
    Q_OBJECT

private slots:
    void test_respiratoryRateBands_data();
    void test_respiratoryRateBands();
    void test_oxygenSaturationScale1Bands_data();
    void test_oxygenSaturationScale1Bands();
    void test_oxygenSaturationScale2OnlyScoresHighValuesWithOxygen_data();
    void test_oxygenSaturationScale2OnlyScoresHighValuesWithOxygen();
    void test_systolicPressureBands_data();
    void test_systolicPressureBands();
    void test_heartRateBands_data();
    void test_heartRateBands();
    void test_temperatureBandsRoundToOneDecimal_data();
    void test_temperatureBandsRoundToOneDecimal();
    void test_supplementalOxygenAddsTwoPoints();
    void test_designScenarioTotalsFour();
    void test_missingParametersMakeSnapshotIncomplete();
    void test_partialTotalStillCountsAvailableParameters();
    void test_responseLevelFollowsAggregateThresholds_data();
    void test_responseLevelFollowsAggregateThresholds();
    void test_singleParameterThreeRaisesResponseBelowFive();
    void test_monitoringIntervalFollowsProtocol_data();
    void test_monitoringIntervalFollowsProtocol();

private:
    static News2::Inputs designScenario();
};

News2::Inputs News2ScoreTest::designScenario()
{
    News2::Inputs inputs;
    inputs.heartRate = 98;
    inputs.oxygenSaturation = 95;
    inputs.respiratoryRate = 21;
    inputs.temperature = 37.2;
    inputs.systolicPressure = 118;
    inputs.isOnSupplementalOxygen = false;
    return inputs;
}

void News2ScoreTest::test_respiratoryRateBands_data()
{
    QTest::addColumn<int>("breathsPerMinute");
    QTest::addColumn<int>("expected");
    QTest::newRow("8 → 3") << 8 << 3;
    QTest::newRow("9 → 1") << 9 << 1;
    QTest::newRow("11 → 1") << 11 << 1;
    QTest::newRow("12 → 0") << 12 << 0;
    QTest::newRow("20 → 0") << 20 << 0;
    QTest::newRow("21 → 2") << 21 << 2;
    QTest::newRow("24 → 2") << 24 << 2;
    QTest::newRow("25 → 3") << 25 << 3;
}

void News2ScoreTest::test_respiratoryRateBands()
{
    QFETCH(int, breathsPerMinute);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreRespiratoryRate(breathsPerMinute), expected);
}

void News2ScoreTest::test_oxygenSaturationScale1Bands_data()
{
    QTest::addColumn<int>("saturation");
    QTest::addColumn<int>("expected");
    QTest::newRow("91 → 3") << 91 << 3;
    QTest::newRow("92 → 2") << 92 << 2;
    QTest::newRow("93 → 2") << 93 << 2;
    QTest::newRow("94 → 1") << 94 << 1;
    QTest::newRow("95 → 1") << 95 << 1;
    QTest::newRow("96 → 0") << 96 << 0;
}

void News2ScoreTest::test_oxygenSaturationScale1Bands()
{
    QFETCH(int, saturation);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreOxygenSaturationScale1(saturation), expected);
}

void News2ScoreTest::test_oxygenSaturationScale2OnlyScoresHighValuesWithOxygen_data()
{
    QTest::addColumn<int>("saturation");
    QTest::addColumn<bool>("isOnOxygen");
    QTest::addColumn<int>("expected");
    QTest::newRow("83 → 3") << 83 << false << 3;
    QTest::newRow("85 → 2") << 85 << false << 2;
    QTest::newRow("87 → 1") << 87 << false << 1;
    QTest::newRow("88–92 → 0") << 90 << true << 0;
    QTest::newRow("97 em ar → 0") << 97 << false << 0;
    QTest::newRow("93 com O2 → 1") << 93 << true << 1;
    QTest::newRow("96 com O2 → 2") << 96 << true << 2;
    QTest::newRow("97 com O2 → 3") << 97 << true << 3;
}

void News2ScoreTest::test_oxygenSaturationScale2OnlyScoresHighValuesWithOxygen()
{
    QFETCH(int, saturation);
    QFETCH(bool, isOnOxygen);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreOxygenSaturationScale2(saturation, isOnOxygen), expected);
}

void News2ScoreTest::test_systolicPressureBands_data()
{
    QTest::addColumn<int>("systolic");
    QTest::addColumn<int>("expected");
    QTest::newRow("90 → 3") << 90 << 3;
    QTest::newRow("91 → 2") << 91 << 2;
    QTest::newRow("101 → 1") << 101 << 1;
    QTest::newRow("111 → 0") << 111 << 0;
    QTest::newRow("219 → 0") << 219 << 0;
    QTest::newRow("220 → 3") << 220 << 3;
}

void News2ScoreTest::test_systolicPressureBands()
{
    QFETCH(int, systolic);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreSystolicPressure(systolic), expected);
}

void News2ScoreTest::test_heartRateBands_data()
{
    QTest::addColumn<int>("beatsPerMinute");
    QTest::addColumn<int>("expected");
    QTest::newRow("40 → 3") << 40 << 3;
    QTest::newRow("41 → 1") << 41 << 1;
    QTest::newRow("51 → 0") << 51 << 0;
    QTest::newRow("90 → 0") << 90 << 0;
    QTest::newRow("91 → 1") << 91 << 1;
    QTest::newRow("111 → 2") << 111 << 2;
    QTest::newRow("131 → 3") << 131 << 3;
}

void News2ScoreTest::test_heartRateBands()
{
    QFETCH(int, beatsPerMinute);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreHeartRate(beatsPerMinute), expected);
}

void News2ScoreTest::test_temperatureBandsRoundToOneDecimal_data()
{
    QTest::addColumn<double>("celsius");
    QTest::addColumn<int>("expected");
    QTest::newRow("35,0 → 3") << 35.0 << 3;
    QTest::newRow("35,1 → 1") << 35.1 << 1;
    QTest::newRow("36,0 → 1") << 36.0 << 1;
    QTest::newRow("36,1 → 0") << 36.1 << 0;
    QTest::newRow("38,0 → 0") << 38.0 << 0;
    QTest::newRow("38,04 arredonda para 38,0 → 0") << 38.04 << 0;
    QTest::newRow("38,1 → 1") << 38.1 << 1;
    QTest::newRow("39,1 → 2") << 39.1 << 2;
}

void News2ScoreTest::test_temperatureBandsRoundToOneDecimal()
{
    QFETCH(double, celsius);
    QFETCH(int, expected);
    QCOMPARE(News2::scoreTemperature(celsius), expected);
}

void News2ScoreTest::test_supplementalOxygenAddsTwoPoints()
{
    QCOMPARE(News2::scoreSupplementalOxygen(true), 2);
    QCOMPARE(News2::scoreSupplementalOxygen(false), 0);
}

void News2ScoreTest::test_designScenarioTotalsFour()
{
    const News2::Snapshot snapshot = News2::evaluate(designScenario(), 0);

    QVERIFY(snapshot.isComplete);
    QCOMPARE(snapshot.total, 4);
    QCOMPARE(snapshot.subscore(Parameter::HeartRate), 1);
    QCOMPARE(snapshot.subscore(Parameter::OxygenSaturation), 1);
    QCOMPARE(snapshot.subscore(Parameter::RespiratoryRate), 2);
    QCOMPARE(News2::responseLevel(snapshot), ResponseLevel::None);
}

void News2ScoreTest::test_missingParametersMakeSnapshotIncomplete()
{
    News2::Inputs inputs = designScenario();
    inputs.systolicPressure.reset();

    const News2::Snapshot snapshot = News2::evaluate(inputs, 0);

    QVERIFY(!snapshot.isComplete);
    QVERIFY(!snapshot.subscore(Parameter::SystolicPressure).has_value());
}

void News2ScoreTest::test_partialTotalStillCountsAvailableParameters()
{
    News2::Inputs inputs = designScenario();
    inputs.heartRate.reset();
    inputs.oxygenSaturation.reset();
    inputs.respiratoryRate.reset();
    inputs.temperature = 39.5;
    inputs.systolicPressure = 85;

    const News2::Snapshot snapshot = News2::evaluate(inputs, 0);

    QVERIFY(!snapshot.isComplete);
    QCOMPARE(snapshot.total, 5);
    QCOMPARE(News2::responseLevel(snapshot), ResponseLevel::Urgent);
}

void News2ScoreTest::test_responseLevelFollowsAggregateThresholds_data()
{
    QTest::addColumn<int>("respiratoryRate");
    QTest::addColumn<int>("heartRate");
    QTest::addColumn<int>("expectedTotal");
    QTest::addColumn<ResponseLevel>("expectedLevel");
    QTest::newRow("FR 24 → total 4") << 24 << 98 << 4 << ResponseLevel::None;
    QTest::newRow("FR 25 → total 5") << 25 << 98 << 5 << ResponseLevel::Urgent;
    QTest::newRow("FR 25, FC 114 → total 6") << 25 << 114 << 6 << ResponseLevel::Urgent;
    QTest::newRow("FR 25, FC 131 → total 7") << 25 << 131 << 7 << ResponseLevel::Emergency;
}

void News2ScoreTest::test_responseLevelFollowsAggregateThresholds()
{
    QFETCH(int, respiratoryRate);
    QFETCH(int, heartRate);
    QFETCH(int, expectedTotal);
    QFETCH(ResponseLevel, expectedLevel);
    News2::Inputs inputs = designScenario();
    inputs.respiratoryRate = respiratoryRate;
    inputs.heartRate = heartRate;

    const News2::Snapshot snapshot = News2::evaluate(inputs, 0);

    QCOMPARE(snapshot.total, expectedTotal);
    QCOMPARE(News2::responseLevel(snapshot), expectedLevel);
}

void News2ScoreTest::test_singleParameterThreeRaisesResponseBelowFive()
{
    News2::Inputs inputs = designScenario();
    inputs.heartRate = 80;
    inputs.oxygenSaturation = 97;
    inputs.respiratoryRate = 16;
    inputs.systolicPressure = 88;

    const News2::Snapshot snapshot = News2::evaluate(inputs, 0);

    QCOMPARE(snapshot.total, 3);
    QCOMPARE(snapshot.parameterScoringThree, Parameter::SystolicPressure);
    QCOMPARE(News2::responseLevel(snapshot), ResponseLevel::SingleParameter);
    QCOMPARE(News2::monitoringIntervalMinutes(snapshot), 60);
}

void News2ScoreTest::test_monitoringIntervalFollowsProtocol_data()
{
    QTest::addColumn<int>("respiratoryRate");
    QTest::addColumn<int>("heartRate");
    QTest::addColumn<int>("oxygenSaturation");
    QTest::addColumn<int>("expectedMinutes");
    QTest::newRow("total 0 → 12 h") << 16 << 80 << 97 << 720;
    QTest::newRow("total 4 → 4–6 h") << 21 << 98 << 95 << 240;
    QTest::newRow("total 5 → 1 h") << 25 << 98 << 95 << 60;
    QTest::newRow("total ≥ 7 → contínuo") << 25 << 131 << 95 << 0;
}

void News2ScoreTest::test_monitoringIntervalFollowsProtocol()
{
    QFETCH(int, respiratoryRate);
    QFETCH(int, heartRate);
    QFETCH(int, oxygenSaturation);
    QFETCH(int, expectedMinutes);
    News2::Inputs inputs = designScenario();
    inputs.respiratoryRate = respiratoryRate;
    inputs.heartRate = heartRate;
    inputs.oxygenSaturation = oxygenSaturation;

    QCOMPARE(News2::monitoringIntervalMinutes(News2::evaluate(inputs, 0)), expectedMinutes);
}

QTEST_APPLESS_MAIN(News2ScoreTest)
#include "tst_news2score.moc"
