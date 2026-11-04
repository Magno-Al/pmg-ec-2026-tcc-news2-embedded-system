#pragma once

#include "core/LiveSignalBuffer.h"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QQuickPaintedItem>
#include <QTimer>

// Curva em varredura, como nos monitores clínicos: a cada quadro desenha só os pixels novos e
// apaga uma faixa à frente — barato no Raspberry Pi. Com liveChannel apontando para um canal
// que tem amostras do sensor (LiveSignalBuffer), desenha o sinal real; sem amostras (build com
// mock), a forma de onda é sintetizada a partir da frequência.
class WaveformItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor idleColor READ idleColor WRITE setIdleColor NOTIFY idleColorChanged)
    Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(bool isActive READ isActive WRITE setIsActive NOTIFY isActiveChanged)
    Q_PROPERTY(double phaseOffset READ phaseOffset WRITE setPhaseOffset NOTIFY phaseOffsetChanged)
    Q_PROPERTY(LiveChannel liveChannel READ liveChannel WRITE setLiveChannel
               NOTIFY liveChannelChanged)

public:
    enum class Shape
    {
        Pulse,
        Plethysmography,
        Respiration
    };
    Q_ENUM(Shape)

    enum class LiveChannel
    {
        None,
        Plethysmography,
        Respiration
    };
    Q_ENUM(LiveChannel)

    explicit WaveformItem(QQuickItem* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);
    QColor idleColor() const { return m_idleColor; }
    void setIdleColor(const QColor& color);
    Shape shape() const { return m_shape; }
    void setShape(Shape shape);
    double rate() const { return m_rate; }
    void setRate(double rate);
    bool isActive() const { return m_isActive; }
    void setIsActive(bool isActive);
    double phaseOffset() const { return m_phase; }
    void setPhaseOffset(double phaseOffset);
    LiveChannel liveChannel() const { return m_liveChannel; }
    void setLiveChannel(LiveChannel liveChannel);

    void paint(QPainter* painter) override;

signals:
    void colorChanged();
    void idleColorChanged();
    void shapeChanged();
    void rateChanged();
    void isActiveChanged();
    void phaseOffsetChanged();
    void liveChannelChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void advance();
    double sampleAt(double phase) const;
    const LiveSignalBuffer* liveBuffer() const;
    double nextLiveValue(const LiveSignalBuffer& buffer, int speed);
    int sweepSpeedPixelsPerSecond() const;

    QColor m_color = Qt::green;
    QColor m_idleColor = Qt::gray;
    Shape m_shape = Shape::Pulse;
    double m_rate = 60.0;
    bool m_isActive = true;
    LiveChannel m_liveChannel = LiveChannel::None;
    double m_liveReadIndex = -1.0;

    QImage m_canvas;
    QTimer m_frameTimer;
    QElapsedTimer m_frameClock;
    double m_phase = 0.0;
    double m_pendingPixels = 0.0;
    int m_cursorX = 0;
    double m_lastY = -1.0;
};
