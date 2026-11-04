#include "AlarmPolicy.h"

namespace AlarmPolicy
{

qint64 muteDurationMilliseconds(News2::ResponseLevel level)
{
    return level == News2::ResponseLevel::Emergency ? 60'000 : 120'000;
}

bool shouldCancelMute(News2::ResponseLevel previousLevel, News2::ResponseLevel currentLevel)
{
    return static_cast<int>(currentLevel) > static_cast<int>(previousLevel);
}

}
