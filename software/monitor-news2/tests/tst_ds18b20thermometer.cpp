#include "sensors/Ds18b20Thermometer.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace
{

// Monta um /sys/bus/w1/devices falso: o mestre do barramento e, opcionalmente, um DS18B20
// com o conteúdo de w1_slave dado.
void writeBus(const QTemporaryDir& bus, const QByteArray& w1Slave)
{
    QDir(bus.path()).mkdir(QStringLiteral("w1_bus_master1"));
    if(w1Slave.isEmpty())
    {
        return;
    }
    QDir(bus.path()).mkdir(QStringLiteral("28-0000006c431f"));
    QFile file(bus.path() + QStringLiteral("/28-0000006c431f/w1_slave"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(w1Slave);
}

const QByteArray ValidReading =
    "e4 01 7f 80 7f ff 0c 10 1c : crc=1c YES\ne4 01 7f 80 7f ff 0c 10 1c t=30250\n";

}

class Ds18b20ThermometerTest : public QObject
{
    Q_OBJECT

private slots:
    void test_readsTheSensorOnTheBus();
    void test_rejectsReadingWithCrcFailure();
    void test_rejectsPowerOnResetValue();
    void test_withoutSensorThereIsNoReading();
    void test_emitsReadingsPeriodicallyFromItsOwnThread();
};

void Ds18b20ThermometerTest::test_readsTheSensorOnTheBus()
{
    QTemporaryDir bus;
    writeBus(bus, ValidReading);

    const std::optional<double> celsius = Ds18b20Thermometer::readOnce(bus.path());

    QVERIFY(celsius);
    QCOMPARE(*celsius, 30.25);
}

void Ds18b20ThermometerTest::test_rejectsReadingWithCrcFailure()
{
    QTemporaryDir bus;
    writeBus(bus, "e4 01 7f 80 7f ff 0c 10 1c : crc=1d NO\ne4 01 7f 80 7f ff 0c 10 1c t=30250\n");

    QVERIFY(!Ds18b20Thermometer::readOnce(bus.path()));
}

void Ds18b20ThermometerTest::test_rejectsPowerOnResetValue()
{
    QTemporaryDir bus;
    writeBus(bus, "50 05 4b 46 7f ff 0c 10 1c : crc=1c YES\n50 05 4b 46 7f ff 0c 10 1c t=85000\n");

    QVERIFY(!Ds18b20Thermometer::readOnce(bus.path()));
}

void Ds18b20ThermometerTest::test_withoutSensorThereIsNoReading()
{
    QTemporaryDir bus;
    writeBus(bus, QByteArray());

    QVERIFY(!Ds18b20Thermometer::readOnce(bus.path()));
}

void Ds18b20ThermometerTest::test_emitsReadingsPeriodicallyFromItsOwnThread()
{
    QTemporaryDir bus;
    writeBus(bus, ValidReading);
    Ds18b20Thermometer thermometer(bus.path(), 50);
    QSignalSpy measured(&thermometer, &Ds18b20Thermometer::temperatureMeasured);

    thermometer.start();

    QTRY_VERIFY_WITH_TIMEOUT(measured.count() >= 3, 2000);
    QCOMPARE(measured.first().first().toDouble(), 30.25);
}

QTEST_GUILESS_MAIN(Ds18b20ThermometerTest)
#include "tst_ds18b20thermometer.moc"
