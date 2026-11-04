#include "core/SpokenNumber.h"

#include <QTest>

class SpokenNumberTest : public QObject
{
    Q_OBJECT

private slots:
    void test_readsSystolicValuesInPortuguese_data();
    void test_readsSystolicValuesInPortuguese();
};

void SpokenNumberTest::test_readsSystolicValuesInPortuguese_data()
{
    QTest::addColumn<int>("value");
    QTest::addColumn<QString>("expected");
    QTest::newRow("50") << 50 << QStringLiteral("cinquenta");
    QTest::newRow("100") << 100 << QStringLiteral("cem");
    QTest::newRow("101") << 101 << QStringLiteral("cento e um");
    QTest::newRow("118") << 118 << QStringLiteral("cento e dezoito");
    QTest::newRow("120") << 120 << QStringLiteral("cento e vinte");
    QTest::newRow("187") << 187 << QStringLiteral("cento e oitenta e sete");
    QTest::newRow("200") << 200 << QStringLiteral("duzentos");
    QTest::newRow("300") << 300 << QStringLiteral("trezentos");
}

void SpokenNumberTest::test_readsSystolicValuesInPortuguese()
{
    QFETCH(int, value);
    QFETCH(QString, expected);
    QCOMPARE(SpokenNumber::toPortuguese(value), expected);
}

QTEST_APPLESS_MAIN(SpokenNumberTest)
#include "tst_spokennumber.moc"
