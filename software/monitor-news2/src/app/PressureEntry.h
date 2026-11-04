#pragma once

#include <QObject>
#include <QQmlEngine>

// Estado do teclado numérico do registro manual de PA sistólica.
class PressureEntry : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString digits READ digits NOTIFY changed)
    Q_PROPERTY(int value READ value NOTIFY changed)
    Q_PROPERTY(bool isInRange READ isInRange NOTIFY changed)
    Q_PROPERTY(int oxygenChoice READ oxygenChoice WRITE setOxygenChoice NOTIFY changed)
    Q_PROPERTY(bool canConfirm READ canConfirm NOTIFY changed)
    Q_PROPERTY(QString valueInWords READ valueInWords NOTIFY changed)

public:
    static constexpr int MinimumSystolic = 50;
    static constexpr int MaximumSystolic = 300;

    using QObject::QObject;

    QString digits() const { return m_digits; }
    int value() const;
    bool isInRange() const;
    int oxygenChoice() const { return m_oxygenChoice; }
    void setOxygenChoice(int choice);
    bool canConfirm() const;
    QString valueInWords() const;

    Q_INVOKABLE void pressKey(const QString& key);
    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    QString m_digits;
    int m_oxygenChoice = -1;
};
