#include "core/PlethysmographyDisplay.h"

#include <QTest>

#include <cmath>

namespace
{

constexpr int SampleRateHz = 100;

}

// Só apresentação: a curva da tela fica em 0,1..0,9 e sobe no pulso (IR filtrado invertido).
class PlethysmographyDisplayTest : public QObject
{
    Q_OBJECT

private slots:
    void test_flatSignalStaysInTheMiddle();
    void test_valuesStayWithinTheDrawingRange();
    void test_lowestInfraredIsTheTopOfTheCurve();
    void test_resetForgetsTheRecentRange();
};

void PlethysmographyDisplayTest::test_flatSignalStaysInTheMiddle()
{
    PlethysmographyDisplay display(SampleRateHz);

    for(int index = 0; index < 3 * SampleRateHz; ++index)
    {
        QCOMPARE(display.add(42.0), 0.5);
    }
}

void PlethysmographyDisplayTest::test_valuesStayWithinTheDrawingRange()
{
    PlethysmographyDisplay display(SampleRateHz);

    for(int index = 0; index < 10 * SampleRateHz; ++index)
    {
        // amplitude crescente: o mínimo e o máximo da janela de 2 s acompanham
        const double seconds = double(index) / SampleRateHz;
        const double value = display.add((1.0 + seconds) * std::sin(2.0 * M_PI * 1.2 * seconds));
        QVERIFY2(value >= 0.1 - 1e-12 && value <= 0.9 + 1e-12, qPrintable(QString::number(value)));
    }
}

void PlethysmographyDisplayTest::test_lowestInfraredIsTheTopOfTheCurve()
{
    PlethysmographyDisplay display(SampleRateHz);
    display.add(0.0);
    display.add(100.0);

    QCOMPARE(display.add(-100.0), 0.9);
    QCOMPARE(display.add(100.0), 0.1);
}

void PlethysmographyDisplayTest::test_resetForgetsTheRecentRange()
{
    // a janela é de 2 s: um valor de 3 s atrás não conta mais, e o reset esquece tudo
    PlethysmographyDisplay display(SampleRateHz);
    display.add(1000.0);
    for(int index = 0; index < 2 * SampleRateHz; ++index)
    {
        display.add(0.0);
    }
    QCOMPARE(display.add(0.0), 0.5);

    display.add(-1000.0);
    display.reset();
    QCOMPARE(display.add(5.0), 0.5);
}

QTEST_APPLESS_MAIN(PlethysmographyDisplayTest)
#include "tst_plethysmographydisplay.moc"
