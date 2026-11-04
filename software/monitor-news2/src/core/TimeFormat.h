#pragma once

#include <QString>

namespace TimeFormat
{

constexpr qint64 Minute = 60'000;
constexpr qint64 Hour = 60 * Minute;

QString hoursMinutes(qint64 epochMilliseconds);
QString hoursMinutesSeconds(qint64 epochMilliseconds);
QString dayMonthHoursMinutes(qint64 epochMilliseconds);

// "47 min", "2 h", "2 h 05"
QString duration(qint64 milliseconds);

// "1:05" — contagem regressiva em minutos e segundos
QString minutesSeconds(qint64 milliseconds);

// "37,2"
QString decimal(double value);

}
