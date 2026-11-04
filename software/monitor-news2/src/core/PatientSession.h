#pragma once

#include "News2Score.h"
#include "VitalSigns.h"

#include <QString>
#include <QVector>

#include <optional>

struct PatientInfo
{
    int age = 0;
    QString ward;
    QString bed;
};

struct PressureRecord
{
    int systolic = 0;
    qint64 recordedAt = 0;
};

struct OxygenRecord
{
    bool isOnSupplementalOxygen = false;
    qint64 recordedAt = 0;
};

enum class EventKind
{
    Score,
    Alarm,
    Prediction,
    Registration,
    Staff,
    Technical
};

struct ClinicalEvent
{
    qint64 at = 0;
    QString text;
    EventKind kind = EventKind::Staff;
};

struct ScoreLogEntry
{
    qint64 at = 0;
    int total = 0;
    bool isComplete = false;
    int intervalMinutes = 0;
};

struct PredictionState
{
    std::optional<int> probability;
    std::optional<int> previousProbability;
    qint64 lastRunAt = 0;
};

// Registros de um plantão. Fora de um plantão nada é registrado: o monitor segue mostrando
// sinais e escore ao vivo, mas não há log, histórico, evolução nem janela para a predição.
struct PatientSession
{
    PatientInfo patient;
    std::optional<qint64> shiftStartedAt;

    QVector<PressureRecord> pressureHistory;
    std::optional<OxygenRecord> oxygen;
    bool useOxygenScale2 = false;

    QVector<ClinicalEvent> events;
    QVector<ScoreLogEntry> scoreLog;
    QVector<VitalSample> vitalHistory;
    PredictionState prediction;
};
