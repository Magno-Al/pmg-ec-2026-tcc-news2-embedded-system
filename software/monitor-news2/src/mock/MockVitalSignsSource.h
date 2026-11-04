#pragma once

#include "core/VitalSigns.h"
#include "devices/VitalSignsSource.h"

#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTimer>

// Simula leituras plausíveis em torno de um alvo. Com deterioração ligada, a FR alvo sobe
// 1 irpm a cada intervalo até 25 — é o que empurra o cenário inicial para NEWS2 ≥ 5.
class MockVitalSignsSource : public VitalSignsSource
{
    Q_OBJECT

public:
    explicit MockVitalSignsSource(
        const VitalSigns& target,
        int deteriorationIntervalSeconds,
        QObject* parent = nullptr
    );

    void start() override;

private:
    void emitReading();

    VitalSigns m_target;
    int m_deteriorationIntervalSeconds;
    QTimer m_readingTimer;
    QElapsedTimer m_sinceLastDeterioration;
    QRandomGenerator m_random;
};
