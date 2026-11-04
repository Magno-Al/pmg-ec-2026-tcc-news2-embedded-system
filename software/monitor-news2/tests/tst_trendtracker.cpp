#include "core/TrendTracker.h"

#include <QTest>

using Direction = TrendTracker::Direction;

class TrendTrackerTest : public QObject
{
    Q_OBJECT

private slots:
    void test_startsSteadyUntilWindowFills();
    void test_noiseBelowThresholdKeepsDirection();
    void test_sustainedRiseMarksRisingSinceThatReading();
    void test_continuingInSameDirectionKeepsOriginalSince();
    void test_reversalRestartsSince();
    void test_isolatedSpikeDoesNotMoveMedian();

private:
    static void feed(TrendTracker& tracker, double value, int count, qint64& at);
};

void TrendTrackerTest::feed(TrendTracker& tracker, double value, int count, qint64& at)
{
    for(int index = 0; index < count; ++index)
    {
        at += 2000;
        tracker.addReading(value, at);
    }
}

void TrendTrackerTest::test_startsSteadyUntilWindowFills()
{
    TrendTracker tracker(1.0, 5);
    qint64 at = 0;
    feed(tracker, 21, 2, at);
    feed(tracker, 25, 2, at);

    QCOMPARE(tracker.direction(), Direction::Steady);
}

void TrendTrackerTest::test_noiseBelowThresholdKeepsDirection()
{
    TrendTracker tracker(3.0, 5);
    qint64 at = 0;
    for(double value : {98, 96, 100, 97, 99, 98, 100, 96, 97, 99})
    {
        at += 2000;
        tracker.addReading(value, at);
    }

    QCOMPARE(tracker.direction(), Direction::Steady);
}

void TrendTrackerTest::test_sustainedRiseMarksRisingSinceThatReading()
{
    TrendTracker tracker(1.0, 5);
    qint64 at = 0;
    feed(tracker, 21, 5, at);
    feed(tracker, 22, 3, at);

    QCOMPARE(tracker.direction(), Direction::Rising);
    QCOMPARE(tracker.since(), at);
}

void TrendTrackerTest::test_continuingInSameDirectionKeepsOriginalSince()
{
    TrendTracker tracker(1.0, 5);
    qint64 at = 0;
    feed(tracker, 21, 5, at);
    feed(tracker, 22, 3, at);
    const qint64 riseStartedAt = at;
    feed(tracker, 22, 2, at);
    feed(tracker, 23, 5, at);

    QCOMPARE(tracker.direction(), Direction::Rising);
    QCOMPARE(tracker.since(), riseStartedAt);
}

void TrendTrackerTest::test_reversalRestartsSince()
{
    TrendTracker tracker(1.0, 5);
    qint64 at = 0;
    feed(tracker, 94, 5, at);
    feed(tracker, 95, 5, at);
    feed(tracker, 94, 3, at);

    QCOMPARE(tracker.direction(), Direction::Falling);
    QCOMPARE(tracker.since(), at);
}

void TrendTrackerTest::test_isolatedSpikeDoesNotMoveMedian()
{
    TrendTracker tracker(1.0, 5);
    qint64 at = 0;
    feed(tracker, 21, 5, at);
    feed(tracker, 30, 1, at);
    feed(tracker, 21, 4, at);

    QCOMPARE(tracker.direction(), Direction::Steady);
}

QTEST_APPLESS_MAIN(TrendTrackerTest)
#include "tst_trendtracker.moc"
