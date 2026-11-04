#include "PressureEntry.h"

#include "core/SpokenNumber.h"

int PressureEntry::value() const
{
    return m_digits.isEmpty() ? 0 : m_digits.toInt();
}

bool PressureEntry::isInRange() const
{
    const int systolic = value();
    return m_digits.size() >= 2 && systolic >= MinimumSystolic && systolic <= MaximumSystolic;
}

void PressureEntry::setOxygenChoice(int choice)
{
    if(choice == m_oxygenChoice)
    {
        return;
    }
    m_oxygenChoice = choice;
    emit changed();
}

bool PressureEntry::canConfirm() const
{
    return isInRange() && m_oxygenChoice >= 0;
}

QString PressureEntry::valueInWords() const
{
    return m_digits.isEmpty() ? QString() : SpokenNumber::toPortuguese(value());
}

void PressureEntry::pressKey(const QString& key)
{
    if(key == QStringLiteral("⌫"))
    {
        m_digits.chop(1);
    }
    else if(key == QStringLiteral("Limpar"))
    {
        m_digits.clear();
    }
    else
    {
        const bool isLeadingZero = m_digits.isEmpty() && key == QStringLiteral("0");
        if(m_digits.size() >= 3 || isLeadingZero)
        {
            return;
        }
        m_digits += key;
    }
    emit changed();
}

void PressureEntry::reset()
{
    m_digits.clear();
    m_oxygenChoice = -1;
    emit changed();
}
