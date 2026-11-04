#pragma once

#include <QColor>
#include <QQuickPaintedItem>

// Contorno tracejado ou duplo. A procedência do dado aparece na borda: sólida (medido),
// dupla (registrado), tracejada (estimado ou ausente). Rectangle só desenha borda sólida.
class FrameItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth NOTIFY appearanceChanged)
    Q_PROPERTY(Style style READ style WRITE setStyle NOTIFY appearanceChanged)

public:
    enum class Style
    {
        Dashed,
        Double
    };
    Q_ENUM(Style)

    explicit FrameItem(QQuickItem* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);
    double lineWidth() const { return m_lineWidth; }
    void setLineWidth(double lineWidth);
    Style style() const { return m_style; }
    void setStyle(Style style);

    void paint(QPainter* painter) override;

signals:
    void appearanceChanged();

private:
    QColor m_color = Qt::white;
    double m_lineWidth = 2.0;
    Style m_style = Style::Dashed;
};
