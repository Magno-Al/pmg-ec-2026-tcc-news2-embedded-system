#include "core/AlarmPolicy.h"

#include <QTest>

using News2::ResponseLevel;

class AlarmPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void test_emergencyMutesForOneMinute();
    void test_otherLevelsMuteForTwoMinutes();
    void test_risingLevelCancelsMute();
    void test_sameOrFallingLevelKeepsMute();
};

void AlarmPolicyTest::test_emergencyMutesForOneMinute()
{
    QCOMPARE(AlarmPolicy::muteDurationMilliseconds(ResponseLevel::Emergency), 60'000);
}

void AlarmPolicyTest::test_otherLevelsMuteForTwoMinutes()
{
    QCOMPARE(AlarmPolicy::muteDurationMilliseconds(ResponseLevel::Urgent), 120'000);
    QCOMPARE(AlarmPolicy::muteDurationMilliseconds(ResponseLevel::SingleParameter), 120'000);
    QCOMPARE(AlarmPolicy::muteDurationMilliseconds(ResponseLevel::None), 120'000);
}

void AlarmPolicyTest::test_risingLevelCancelsMute()
{
    QVERIFY(AlarmPolicy::shouldCancelMute(ResponseLevel::Urgent, ResponseLevel::Emergency));
    QVERIFY(AlarmPolicy::shouldCancelMute(ResponseLevel::None, ResponseLevel::SingleParameter));
}

void AlarmPolicyTest::test_sameOrFallingLevelKeepsMute()
{
    QVERIFY(!AlarmPolicy::shouldCancelMute(ResponseLevel::Urgent, ResponseLevel::Urgent));
    QVERIFY(!AlarmPolicy::shouldCancelMute(ResponseLevel::Emergency, ResponseLevel::Urgent));
}

QTEST_APPLESS_MAIN(AlarmPolicyTest)
#include "tst_alarmpolicy.moc"
