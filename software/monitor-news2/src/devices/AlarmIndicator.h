#pragma once

#include <QString>

// Fronteira com o LED RGB e o buzzer do equipamento (GPIO/PWM na implementação real).
enum class LedPattern
{
    Off,
    SteadyGreen,
    SteadyWhite,
    SteadyOrange,
    BlinkingOrange,
    SteadyRed,
    BlinkingRed
};

enum class BuzzerPattern
{
    Silent,
    LowPriority,
    MediumPriority,
    HighPriority
};

class AlarmIndicator
{
public:
    virtual ~AlarmIndicator() = default;

    virtual void apply(LedPattern led, BuzzerPattern buzzer) = 0;
};
