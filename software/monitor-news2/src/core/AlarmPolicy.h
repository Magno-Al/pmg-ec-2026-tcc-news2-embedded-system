#pragma once

#include "News2Score.h"

// Fadiga de alarme: o som só pausa por tempo limitado, nunca de forma permanente
// (prioridades no padrão IEC 60601-1-8).
namespace AlarmPolicy
{

qint64 muteDurationMilliseconds(News2::ResponseLevel level);

bool shouldCancelMute(News2::ResponseLevel previousLevel, News2::ResponseLevel currentLevel);

}
