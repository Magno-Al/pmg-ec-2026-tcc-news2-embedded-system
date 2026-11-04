#include "WaveformItem.h"

#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace
{

constexpr int FrameMilliseconds = 33;
constexpr int EraseAheadPixels = 16;
constexpr double LineWidth = 2.0;
// O sensor entrega amostras em rajadas (o PPG a cada 40 ms; a respiração a cada batimento,
// ~0,8 s): a curva lê com atraso fixo para nunca alcançar a escrita, e ressincroniza se ficar
// para trás demais.
constexpr double LiveDelaySeconds = 0.4;
constexpr int LiveMinimumDelaySamples = 8;
constexpr double LiveMaximumLagSeconds = 3.0;

double gaussian(double phase, double center, double width)
{
    const double distance = (phase - center) / width;
    return std::exp(-distance * distance);
}

}

WaveformItem::WaveformItem(QQuickItem* parent)
    : QQuickPaintedItem{parent}
{
    setOpaquePainting(false);
    m_frameTimer.setInterval(FrameMilliseconds);
    connect(&m_frameTimer, &QTimer::timeout, this, &WaveformItem::advance);
    m_frameClock.start();
    m_frameTimer.start();
}

void WaveformItem::setColor(const QColor& color)
{
    if(color == m_color)
    {
        return;
    }
    m_color = color;
    emit colorChanged();
}

void WaveformItem::setIdleColor(const QColor& color)
{
    if(color == m_idleColor)
    {
        return;
    }
    m_idleColor = color;
    emit idleColorChanged();
}

void WaveformItem::setShape(Shape shape)
{
    if(shape == m_shape)
    {
        return;
    }
    m_shape = shape;
    emit shapeChanged();
}

void WaveformItem::setRate(double rate)
{
    if(qFuzzyCompare(rate, m_rate))
    {
        return;
    }
    m_rate = rate;
    emit rateChanged();
}

void WaveformItem::setIsActive(bool isActive)
{
    if(isActive == m_isActive)
    {
        return;
    }
    m_isActive = isActive;
    m_liveReadIndex = -1.0;
    emit isActiveChanged();
}

void WaveformItem::setPhaseOffset(double phaseOffset)
{
    m_phase = phaseOffset;
    emit phaseOffsetChanged();
}

void WaveformItem::setLiveChannel(LiveChannel liveChannel)
{
    if(liveChannel == m_liveChannel)
    {
        return;
    }
    m_liveChannel = liveChannel;
    m_liveReadIndex = -1.0;
    emit liveChannelChanged();
}

const LiveSignalBuffer* WaveformItem::liveBuffer() const
{
    if(m_liveChannel == LiveChannel::None)
    {
        return nullptr;
    }
    const LiveSignalBuffer& buffer = LiveSignalBuffer::forChannel(
        m_liveChannel == LiveChannel::Plethysmography
            ? LiveSignalBuffer::Channel::Plethysmography
            : LiveSignalBuffer::Channel::Respiration
    );
    return buffer.hasData() ? &buffer : nullptr;
}

double WaveformItem::nextLiveValue(const LiveSignalBuffer& buffer, int speed)
{
    const double rate = buffer.sampleRateHz();
    const double newest = double(buffer.writtenCount() - 1);
    const double delay = std::max(LiveDelaySeconds * rate, double(LiveMinimumDelaySamples));
    if(m_liveReadIndex < 0.0 || newest - m_liveReadIndex > LiveMaximumLagSeconds * rate)
    {
        m_liveReadIndex = newest - delay;
    }
    m_liveReadIndex = std::min(m_liveReadIndex + rate / speed, newest);
    return buffer.valueAt(m_liveReadIndex);
}

void WaveformItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    const QSize size = newGeometry.size().toSize();
    if(size.isEmpty() || size == m_canvas.size())
    {
        return;
    }
    m_canvas = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_canvas.fill(Qt::transparent);
    m_cursorX = 0;
    m_lastY = -1.0;
}

int WaveformItem::sweepSpeedPixelsPerSecond() const
{
    return m_shape == Shape::Respiration ? 30 : 110;
}

double WaveformItem::sampleAt(double phase) const
{
    if(!m_isActive)
    {
        return 0.5 + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.02;
    }
    switch(m_shape)
    {
        case Shape::Pulse:
            return gaussian(phase, 0.14, 0.045) + 0.28 * gaussian(phase, 0.40, 0.05);
        case Shape::Plethysmography:
            return 0.95 * gaussian(phase, 0.22, 0.10) + 0.35 * gaussian(phase, 0.52, 0.08);
        case Shape::Respiration:
            return 0.5 - 0.5 * std::cos(2 * M_PI * phase) + 0.05 * std::sin(4 * M_PI * phase);
    }
    return 0.5;
}

void WaveformItem::advance()
{
    const double elapsedSeconds = std::min(0.1, m_frameClock.restart() / 1000.0);
    if(m_canvas.isNull() || !isVisible())
    {
        return;
    }

    const int width = m_canvas.width();
    const int height = m_canvas.height();
    const int topMargin = height > 90 ? 30 : height > 60 ? 26 : 8;
    const int bottomMargin = height > 90 ? 22 : 6;
    const int speed = sweepSpeedPixelsPerSecond();

    m_pendingPixels += speed * elapsedSeconds;
    const int newPixels = int(m_pendingPixels);
    if(newPixels == 0)
    {
        return;
    }
    m_pendingPixels -= newPixels;

    QPainter painter(&m_canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(m_isActive ? m_color : m_idleColor, LineWidth, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));

    const auto eraseColumns = [&painter, height](int fromX, int count) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(QRect(fromX, 0, count, height), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    };
    eraseColumns(m_cursorX, std::min(EraseAheadPixels + newPixels, width - m_cursorX));

    const LiveSignalBuffer* live = m_isActive ? liveBuffer() : nullptr;
    for(int pixel = 0; pixel < newPixels; ++pixel)
    {
        double value = 0.0;
        if(live)
        {
            value = nextLiveValue(*live, speed);
        }
        else
        {
            m_phase = std::fmod(m_phase + (m_rate / 60.0) / speed, 1.0);
            value = sampleAt(m_phase);
        }
        const double y = topMargin + (1.0 - value) * (height - topMargin - bottomMargin);
        if(m_lastY >= 0.0)
        {
            painter.drawLine(QPointF(m_cursorX - 1, m_lastY), QPointF(m_cursorX, y));
        }
        m_lastY = y;
        ++m_cursorX;
        if(m_cursorX >= width)
        {
            m_cursorX = 0;
            m_lastY = -1.0;
            eraseColumns(0, EraseAheadPixels);
        }
    }
    update();
}

void WaveformItem::paint(QPainter* painter)
{
    if(!m_canvas.isNull())
    {
        painter->drawImage(0, 0, m_canvas);
    }
}
