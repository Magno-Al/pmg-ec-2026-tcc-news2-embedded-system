#pragma once

#include "devices/AlarmIndicator.h"

#include <optional>
#include <utility>

// Registra no console as mudanças de LED e buzzer, no lugar do GPIO.
class ConsoleAlarmIndicator : public AlarmIndicator
{
public:
    void apply(LedPattern led, BuzzerPattern buzzer) override;

private:
    std::optional<std::pair<LedPattern, BuzzerPattern>> m_lastApplied;
};
