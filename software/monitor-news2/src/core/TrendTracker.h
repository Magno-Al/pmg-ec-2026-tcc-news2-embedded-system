#pragma once

#include <QVector>

// Direção de tendência de um sinal vital contínuo. Usa a mediana das últimas leituras para não
// reagir ao ruído do sensor: a direção muda quando a mediana se afasta do último patamar por
// pelo menos o limiar; "since" marca o início da direção atual.
class TrendTracker
{
public:
    enum class Direction
    {
        Falling = -1,
        Steady = 0,
        Rising = 1
    };

    TrendTracker(double threshold, int windowSize);

    void addReading(double value, qint64 at);

    Direction direction() const { return m_direction; }
    qint64 since() const { return m_since; }

private:
    double median() const;

    double m_threshold;
    int m_windowSize;
    QVector<double> m_window;
    bool m_hasLevel = false;
    double m_level = 0.0;
    Direction m_direction = Direction::Steady;
    qint64 m_since = 0;
};
