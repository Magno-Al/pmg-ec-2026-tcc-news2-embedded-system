#pragma once

#include <QColor>
#include <QQuickPaintedItem>

// Hachura diagonal estática: marca o que vem do modelo (cor do modelo) ou o que está sem sinal
// (cinza). Repinta só quando muda de tamanho ou cor — nenhum gradiente animado.
class HatchItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged)

public:
    explicit HatchItem(QQuickItem* parent = nullptr);

    QColor lineColor() const { return m_lineColor; }
    void setLineColor(const QColor& color);

    void paint(QPainter* painter) override;

signals:
    void lineColorChanged();

private:
    QColor m_lineColor = Qt::gray;
};
