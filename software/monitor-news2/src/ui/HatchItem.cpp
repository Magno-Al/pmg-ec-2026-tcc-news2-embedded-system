#include "HatchItem.h"

#include <QPainter>

namespace
{

constexpr int Spacing = 12;
constexpr double StrokeWidth = 2.0;

}

HatchItem::HatchItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
}

void HatchItem::setLineColor(const QColor& color)
{
    if(color == m_lineColor)
    {
        return;
    }
    m_lineColor = color;
    emit lineColorChanged();
    update();
}

void HatchItem::paint(QPainter* painter)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(m_lineColor, StrokeWidth));
    const int extent = int(width() + height());
    for(int offset = 0; offset < extent; offset += Spacing)
    {
        painter->drawLine(QPointF(offset, 0), QPointF(offset - height(), height()));
    }
}
