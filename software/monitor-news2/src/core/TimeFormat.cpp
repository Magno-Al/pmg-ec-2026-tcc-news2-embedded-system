#include "TimeFormat.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>

namespace TimeFormat
{

namespace
{

QString twoDigits(qint64 value)
{
    return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
}

}

QString hoursMinutes(qint64 epochMilliseconds)
{
    return QDateTime::fromMSecsSinceEpoch(epochMilliseconds).toString(QStringLiteral("HH:mm"));
}

QString hoursMinutesSeconds(qint64 epochMilliseconds)
{
    return QDateTime::fromMSecsSinceEpoch(epochMilliseconds).toString(QStringLiteral("HH:mm:ss"));
}

QString dayMonthHoursMinutes(qint64 epochMilliseconds)
{
    return QDateTime::fromMSecsSinceEpoch(epochMilliseconds).toString(QStringLiteral("dd/MM HH:mm"));
}

QString duration(qint64 milliseconds)
{
    const qint64 minutes = std::max<qint64>(0, std::llround(double(milliseconds) / Minute));
    if(minutes < 60)
    {
        return QStringLiteral("%1 min").arg(minutes);
    }
    const qint64 hours = minutes / 60;
    const qint64 remainder = minutes % 60;
    if(remainder == 0)
    {
        return QStringLiteral("%1 h").arg(hours);
    }
    return QStringLiteral("%1 h %2").arg(hours).arg(twoDigits(remainder));
}

QString minutesSeconds(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, (milliseconds + 999) / 1000);
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(twoDigits(seconds % 60));
}

QString decimal(double value)
{
    return QString::number(value, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(','));
}

}
