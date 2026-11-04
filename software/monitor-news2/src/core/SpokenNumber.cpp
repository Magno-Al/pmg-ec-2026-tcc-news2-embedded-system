#include "SpokenNumber.h"

#include <QStringList>

namespace SpokenNumber
{

QString toPortuguese(int value)
{
    static const QStringList units = {
        "zero", "um", "dois", "três", "quatro", "cinco", "seis", "sete", "oito", "nove", "dez",
        "onze", "doze", "treze", "quatorze", "quinze", "dezesseis", "dezessete", "dezoito",
        "dezenove"
    };
    static const QStringList tens = {
        "", "", "vinte", "trinta", "quarenta", "cinquenta", "sessenta", "setenta", "oitenta",
        "noventa"
    };
    static const QStringList hundreds = {
        "", "cento", "duzentos", "trezentos", "quatrocentos", "quinhentos", "seiscentos",
        "setecentos", "oitocentos", "novecentos"
    };

    if(value < 0 || value > 999)
    {
        return QString::number(value);
    }
    if(value == 0)
    {
        return units[0];
    }
    if(value == 100)
    {
        return QStringLiteral("cem");
    }

    QStringList parts;
    const int hundred = value / 100;
    const int remainder = value % 100;
    if(hundred > 0)
    {
        parts << hundreds[hundred];
    }
    if(remainder > 0)
    {
        if(remainder < 20)
        {
            parts << units[remainder];
        }
        else
        {
            const int ten = remainder / 10;
            const int unit = remainder % 10;
            parts << (unit > 0 ? tens[ten] + QStringLiteral(" e ") + units[unit] : tens[ten]);
        }
    }
    return parts.join(QStringLiteral(" e "));
}

}
