#include "FrameItem.h"

#include <QPainter>

FrameItem::FrameItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
}

void FrameItem::setColor(const QColor& color)
{
    m_color = color;
    emit appearanceChanged();
    update();
}

void FrameItem::setLineWidth(double lineWidth)
{
    m_lineWidth = lineWidth;
    emit appearanceChanged();
    update();
}

void FrameItem::setStyle(Style style)
{
    m_style = style;
    emit appearanceChanged();
    update();
}

void FrameItem::paint(QPainter* painter)
{
    painter->setBrush(Qt::NoBrush);
    if(m_style == Style::Double)
    {
        const double stroke = std::max(1.0, m_lineWidth / 4.0);
        painter->setPen(QPen(m_color, stroke, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        const double outerInset = stroke / 2.0;
        const double innerInset = m_lineWidth - stroke / 2.0;
        painter->drawRect(QRectF(outerInset, outerInset, width() - 2 * outerInset,
                                 height() - 2 * outerInset));
        painter->drawRect(QRectF(innerInset, innerInset, width() - 2 * innerInset,
                                 height() - 2 * innerInset));
        return;
    }
    QPen pen(m_color, m_lineWidth, Qt::CustomDashLine, Qt::FlatCap, Qt::MiterJoin);
    pen.setDashPattern({3.0, 2.0});
    painter->setPen(pen);
    const double inset = m_lineWidth / 2.0;
    painter->drawRect(QRectF(inset, inset, width() - 2 * inset, height() - 2 * inset));
}
