#include "TrendPlotItem.h"

#include <QPainter>
#include <QPainterPath>
#include <QPointF>

namespace
{

constexpr double HorizontalSpan = 1000.0;
constexpr double VerticalSpan = 100.0;
constexpr double DotSize = 10.0;

}

TrendPlotItem::TrendPlotItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
    setAntialiasing(true);
}

void TrendPlotItem::changeAppearance()
{
    emit appearanceChanged();
    update();
}

void TrendPlotItem::setPoints(const QVariantList& points)
{
    m_points = points;
    changeAppearance();
}

void TrendPlotItem::setColor(const QColor& color)
{
    m_color = color;
    changeAppearance();
}

void TrendPlotItem::setBandColor(const QColor& color)
{
    m_bandColor = color;
    changeAppearance();
}

void TrendPlotItem::setDotFillColor(const QColor& color)
{
    m_dotFillColor = color;
    changeAppearance();
}

void TrendPlotItem::setBandTop(double value)
{
    m_bandTop = value;
    changeAppearance();
}

void TrendPlotItem::setBandBottom(double value)
{
    m_bandBottom = value;
    changeAppearance();
}

void TrendPlotItem::setIsDashed(bool isDashed)
{
    m_isDashed = isDashed;
    changeAppearance();
}

void TrendPlotItem::setHasDots(bool hasDots)
{
    m_hasDots = hasDots;
    changeAppearance();
}

void TrendPlotItem::paint(QPainter* painter)
{
    const double scaleX = width() / HorizontalSpan;
    const double scaleY = height() / VerticalSpan;
    const auto toItem = [scaleX, scaleY](const QPointF& point) {
        return QPointF(point.x() * scaleX, point.y() * scaleY);
    };

    painter->fillRect(
        QRectF(0, m_bandTop * scaleY, width(), (m_bandBottom - m_bandTop) * scaleY),
        m_bandColor
    );

    QPainterPath path;
    QList<QPointF> itemPoints;
    for(const QVariant& value : m_points)
    {
        const QPointF point = toItem(value.toPointF());
        if(itemPoints.isEmpty())
        {
            path.moveTo(point);
        }
        else
        {
            path.lineTo(point);
        }
        itemPoints.append(point);
    }

    painter->setRenderHint(QPainter::Antialiasing);
    QPen pen(m_color, 2.0, m_isDashed ? Qt::CustomDashLine : Qt::SolidLine, Qt::FlatCap,
             Qt::RoundJoin);
    if(m_isDashed)
    {
        pen.setDashPattern({2.0, 2.0});
    }
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    if(m_hasDots)
    {
        painter->setPen(QPen(m_color, 2.0));
        painter->setBrush(m_dotFillColor);
        for(const QPointF& point : itemPoints)
        {
            painter->drawRect(QRectF(point.x() - DotSize / 2, point.y() - DotSize / 2,
                                     DotSize, DotSize));
        }
    }
}
