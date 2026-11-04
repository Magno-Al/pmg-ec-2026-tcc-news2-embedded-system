#include "ConsoleAlarmIndicator.h"

#include <QDebug>

namespace
{

QString describeLed(LedPattern led)
{
    switch(led)
    {
        case LedPattern::Off:
            return QStringLiteral("apagado");
        case LedPattern::SteadyGreen:
            return QStringLiteral("verde fixo");
        case LedPattern::SteadyWhite:
            return QStringLiteral("branco fixo");
        case LedPattern::SteadyOrange:
            return QStringLiteral("laranja fixo");
        case LedPattern::BlinkingOrange:
            return QStringLiteral("laranja piscando");
        case LedPattern::SteadyRed:
            return QStringLiteral("vermelho fixo");
        case LedPattern::BlinkingRed:
            return QStringLiteral("vermelho piscando");
    }
    return {};
}

QString describeBuzzer(BuzzerPattern buzzer)
{
    switch(buzzer)
    {
        case BuzzerPattern::Silent:
            return QStringLiteral("silencioso");
        case BuzzerPattern::LowPriority:
            return QStringLiteral("prioridade baixa · 1 bipe a cada 20 s");
        case BuzzerPattern::MediumPriority:
            return QStringLiteral("prioridade média · 3 pulsos, repete");
        case BuzzerPattern::HighPriority:
            return QStringLiteral("prioridade alta · 10 pulsos, repete");
    }
    return {};
}

}

void ConsoleAlarmIndicator::apply(LedPattern led, BuzzerPattern buzzer)
{
    const auto current = std::make_pair(led, buzzer);
    if(m_lastApplied == current)
    {
        return;
    }
    m_lastApplied = current;
    qInfo().noquote() << "[LED]" << describeLed(led) << "| [BUZZER]" << describeBuzzer(buzzer);
}
