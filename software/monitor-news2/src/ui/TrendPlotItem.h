#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>

// Linha de tendência de um parâmetro. Pontos em coordenadas normalizadas: x de 0 a 1000
// (início a fim do período) e y de 0 a 100 (topo a base do eixo), como devolvidos pelo
// MonitorController::trends. A faixa sombreada marca os valores que pontuam 0 no NEWS2.
class TrendPlotItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY appearanceChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bandColor READ bandColor WRITE setBandColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dotFillColor READ dotFillColor WRITE setDotFillColor
                   NOTIFY appearanceChanged)
    Q_PROPERTY(double bandTop READ bandTop WRITE setBandTop NOTIFY appearanceChanged)
    Q_PROPERTY(double bandBottom READ bandBottom WRITE setBandBottom NOTIFY appearanceChanged)
    Q_PROPERTY(bool isDashed READ isDashed WRITE setIsDashed NOTIFY appearanceChanged)
    Q_PROPERTY(bool hasDots READ hasDots WRITE setHasDots NOTIFY appearanceChanged)

public:
    explicit TrendPlotItem(QQuickItem* parent = nullptr);

    QVariantList points() const { return m_points; }
    void setPoints(const QVariantList& points);
    QColor color() const { return m_color; }
    void setColor(const QColor& color);
    QColor bandColor() const { return m_bandColor; }
    void setBandColor(const QColor& color);
    QColor dotFillColor() const { return m_dotFillColor; }
    void setDotFillColor(const QColor& color);
    double bandTop() const { return m_bandTop; }
    void setBandTop(double value);
    double bandBottom() const { return m_bandBottom; }
    void setBandBottom(double value);
    bool isDashed() const { return m_isDashed; }
    void setIsDashed(bool isDashed);
    bool hasDots() const { return m_hasDots; }
    void setHasDots(bool hasDots);

    void paint(QPainter* painter) override;

signals:
    void appearanceChanged();

private:
    void changeAppearance();

    QVariantList m_points;
    QColor m_color = Qt::white;
    QColor m_bandColor = Qt::darkGreen;
    QColor m_dotFillColor = Qt::black;
    double m_bandTop = 0.0;
    double m_bandBottom = 0.0;
    bool m_isDashed = false;
    bool m_hasDots = false;
};
