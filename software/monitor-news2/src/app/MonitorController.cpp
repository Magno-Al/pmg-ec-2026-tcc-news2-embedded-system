#include "MonitorController.h"

#include "core/AlarmPolicy.h"
#include "core/TimeFormat.h"
#include "devices/AlarmIndicator.h"
#include "devices/VitalSignsSource.h"
#include "prediction/DeteriorationPredictor.h"

#include <QDateTime>
#include <QPointF>

#include <algorithm>
#include <cmath>

using News2::Parameter;
using News2::ResponseLevel;
using TimeFormat::Hour;
using TimeFormat::Minute;

MonitorController* MonitorController::s_instance = nullptr;

namespace
{

constexpr int TickMilliseconds = 500;
constexpr qint64 SnapshotPeriod = 30'000;
constexpr qint64 PredictionPeriod = Hour;
constexpr qint64 PredictionWindow = 6 * Hour;
constexpr qint64 SamplePeriod = Minute;
constexpr qint64 WakeDuration = 30'000;
constexpr qint64 ScoreSlot = 30 * Minute;
constexpr int PredictionPositiveThreshold = 50;
constexpr int UnknownScore = -1;
constexpr int TrendWindowReadings = 15;
constexpr int HandoffSlotCount = 24;

int toInt(ResponseLevel level)
{
    return static_cast<int>(level);
}

int scoreOrUnknown(const std::optional<int>& value)
{
    return value.value_or(UnknownScore);
}

QString shortLabel(Parameter parameter)
{
    switch(parameter)
    {
        case Parameter::RespiratoryRate:
            return QStringLiteral("FR");
        case Parameter::OxygenSaturation:
            return QStringLiteral("SpO₂");
        case Parameter::SupplementalOxygen:
            return QStringLiteral("O₂");
        case Parameter::SystolicPressure:
            return QStringLiteral("PA");
        case Parameter::HeartRate:
            return QStringLiteral("FC");
        case Parameter::Temperature:
            return QStringLiteral("T°");
    }
    return {};
}

QString provenanceOf(Parameter parameter)
{
    switch(parameter)
    {
        case Parameter::SupplementalOxygen:
        case Parameter::SystolicPressure:
            return QStringLiteral("recorded");
        default:
            return QStringLiteral("measured");
    }
}

QString eventKindName(EventKind kind)
{
    switch(kind)
    {
        case EventKind::Score:
            return QStringLiteral("score");
        case EventKind::Alarm:
            return QStringLiteral("alarm");
        case EventKind::Prediction:
            return QStringLiteral("prediction");
        case EventKind::Registration:
            return QStringLiteral("registration");
        case EventKind::Staff:
            return QStringLiteral("staff");
        case EventKind::Technical:
            return QStringLiteral("technical");
    }
    return {};
}

QString oxygenDescription(bool isOnSupplementalOxygen)
{
    return isOnSupplementalOxygen ? QStringLiteral("O₂ suplementar")
                                  : QStringLiteral("ar ambiente");
}

QString alarmEventText(ResponseLevel level)
{
    switch(level)
    {
        case ResponseLevel::SingleParameter:
            return QStringLiteral("Alarme: parâmetro único em 3");
        case ResponseLevel::Urgent:
            return QStringLiteral("Alarme: resposta urgente");
        case ResponseLevel::Emergency:
            return QStringLiteral("Alarme: emergência");
        case ResponseLevel::None:
            break;
    }
    return {};
}

double clampedFraction(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

}

MonitorController::MonitorController(
    PatientSession session,
    VitalSignsSource& vitalSignsSource,
    DeteriorationPredictor& predictor,
    AlarmIndicator& alarmIndicator,
    QObject* parent
)
    : QObject{parent}
    , m_session(std::move(session))
    , m_vitalSignsSource(vitalSignsSource)
    , m_predictor(predictor)
    , m_alarmIndicator(alarmIndicator)
    , m_heartRateTrend(3.0, TrendWindowReadings)
    , m_oxygenSaturationTrend(1.0, TrendWindowReadings)
    , m_respiratoryRateTrend(1.0, TrendWindowReadings)
{
    m_now = QDateTime::currentMSecsSinceEpoch();
    m_lastSampleAt = m_now;

    connect(
        &m_vitalSignsSource, &VitalSignsSource::pulseOximetryMeasured,
        this, &MonitorController::onPulseOximetry
    );
    connect(
        &m_vitalSignsSource, &VitalSignsSource::temperatureMeasured,
        this, &MonitorController::onTemperature
    );
    connect(
        &m_vitalSignsSource, &VitalSignsSource::fingerContactChanged,
        this, &MonitorController::onFingerContactChanged
    );

    m_tickTimer.setInterval(TickMilliseconds);
    connect(&m_tickTimer, &QTimer::timeout, this, &MonitorController::tick);
}

void MonitorController::setInstance(MonitorController* instance)
{
    s_instance = instance;
}

MonitorController* MonitorController::create(QQmlEngine*, QJSEngine* jsEngine)
{
    Q_ASSERT(s_instance);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void MonitorController::start()
{
    m_vitalSignsSource.start();
    takeSnapshot();
    m_previousLevel = currentLevel();
    m_tickTimer.start();
    refreshViews();
}

QString MonitorController::clockText() const
{
    return TimeFormat::hoursMinutesSeconds(m_now);
}

void MonitorController::onPulseOximetry(int heartRate, int oxygenSaturation, int respiratoryRate)
{
    m_hasEverHadContact = true;
    m_lastOptical = OpticalReading{heartRate, oxygenSaturation, respiratoryRate, m_now};
    // 0 = o sensor real ainda não fechou a medida (a FR leva ~60 s): fica fora da tendência
    if(heartRate > 0)
    {
        m_heartRateTrend.addReading(heartRate, m_now);
    }
    if(oxygenSaturation > 0)
    {
        m_oxygenSaturationTrend.addReading(oxygenSaturation, m_now);
    }
    if(respiratoryRate > 0)
    {
        m_respiratoryRateTrend.addReading(respiratoryRate, m_now);
    }
}

void MonitorController::onTemperature(double celsius)
{
    m_temperature = celsius;
}

void MonitorController::onFingerContactChanged(bool hasFingerContact)
{
    if(hasFingerContact == m_hasFingerContact)
    {
        return;
    }
    m_hasFingerContact = hasFingerContact;
    if(hasFingerContact)
    {
        m_lastOptical.reset();
    }
    // Ao ligar sem dedo o monitor só aguarda; "sem contato" é alarme técnico apenas quando
    // havia leitura e o dedo saiu.
    if(!m_hasEverHadContact)
    {
        m_hasEverHadContact = hasFingerContact;
        refreshViews();
        return;
    }
    if(hasFingerContact)
    {
        addEvent(
            QStringLiteral("Sensor reconectado após %1")
                .arg(TimeFormat::minutesSeconds(m_now - m_contactLostAt)),
            EventKind::Technical
        );
    }
    else
    {
        m_contactLostAt = m_now;
        addEvent(
            QStringLiteral("Sensor de dedo sem contato — FC, SpO₂, FR suspensos"),
            EventKind::Technical
        );
    }
    takeSnapshot();
    refreshViews();
}

void MonitorController::tick()
{
    m_now = QDateTime::currentMSecsSinceEpoch();
    m_isBlinkOn = !m_isBlinkOn;

    if(m_now - m_snapshot.takenAt >= SnapshotPeriod)
    {
        takeSnapshot();
    }
    if(isShiftActive())
    {
        if(m_now - m_session.prediction.lastRunAt >= PredictionPeriod)
        {
            runPrediction();
        }
        if(m_now - m_lastSampleAt >= SamplePeriod)
        {
            recordVitalSample();
        }
    }
    refreshViews();
}

News2::Inputs MonitorController::currentInputs() const
{
    News2::Inputs inputs;
    if(m_hasFingerContact && m_lastOptical)
    {
        const auto measured = [](int value) {
            return value > 0 ? std::optional<int>(value) : std::nullopt;
        };
        inputs.heartRate = measured(m_lastOptical->heartRate);
        inputs.oxygenSaturation = measured(m_lastOptical->oxygenSaturation);
        inputs.respiratoryRate = measured(m_lastOptical->respiratoryRate);
    }
    inputs.temperature = m_temperature;
    if(const auto pressureRecord = lastPressure())
    {
        inputs.systolicPressure = pressureRecord->systolic;
    }
    if(m_session.oxygen)
    {
        inputs.isOnSupplementalOxygen = m_session.oxygen->isOnSupplementalOxygen;
    }
    inputs.useOxygenScale2 = m_session.useOxygenScale2;
    return inputs;
}

void MonitorController::takeSnapshot()
{
    applySnapshot(News2::evaluate(currentInputs(), m_now));
}

void MonitorController::applySnapshot(const News2::Snapshot& snapshot)
{
    const News2::Snapshot previous = m_snapshot;
    const ResponseLevel level = News2::responseLevel(snapshot);
    m_snapshot = snapshot;

    if(isShiftActive())
    {
        m_session.scoreLog.append({
            snapshot.takenAt,
            snapshot.total,
            snapshot.isComplete,
            News2::monitoringIntervalMinutes(snapshot)
        });
    }

    const bool isScoreChange = previous.takenAt != 0
        && previous.isComplete
        && snapshot.isComplete
        && previous.total != snapshot.total;
    if(isScoreChange)
    {
        addEvent(
            QStringLiteral("NEWS2 %1 → %2").arg(previous.total).arg(snapshot.total),
            EventKind::Score
        );
    }

    if(AlarmPolicy::shouldCancelMute(m_previousLevel, level))
    {
        m_muteUntil = 0;
        m_alarmSince = snapshot.takenAt;
        addEvent(alarmEventText(level), EventKind::Alarm);
    }
    if(level == ResponseLevel::None && m_previousLevel != ResponseLevel::None)
    {
        m_alarmSince.reset();
        addEvent(QStringLiteral("Alarme clínico encerrado"), EventKind::Staff);
    }
    m_previousLevel = level;
}

void MonitorController::runPrediction()
{
    PredictionState& prediction = m_session.prediction;
    const bool hasFullWindow = m_now - *m_session.shiftStartedAt >= PredictionWindow;

    std::optional<int> probability = prediction.probability;
    if(!hasFullWindow)
    {
        probability.reset();
    }
    else if(m_snapshot.isComplete && m_lastOptical && m_temperature)
    {
        PredictionInput input;
        input.snapshot = m_snapshot;
        input.currentVitals = {
            m_lastOptical->heartRate,
            m_lastOptical->oxygenSaturation,
            m_lastOptical->respiratoryRate,
            *m_temperature
        };
        input.patientAge = m_session.patient.age;
        for(const VitalSample& sample : m_session.vitalHistory)
        {
            if(sample.takenAt >= m_now - PredictionWindow)
            {
                input.lastSixHours.append(sample);
            }
        }
        if(const auto result = m_predictor.predictProbability(input))
        {
            probability = result;
        }
    }

    prediction.previousProbability = prediction.probability;
    prediction.probability = probability;
    prediction.lastRunAt = m_now;

    if(probability)
    {
        const bool isPositive = *probability >= PredictionPositiveThreshold;
        addEvent(
            isPositive ? QStringLiteral("Predição 6 h: risco de piora")
                       : QStringLiteral("Predição 6 h: sem alerta"),
            EventKind::Prediction
        );
    }
}

void MonitorController::recordVitalSample()
{
    m_lastSampleAt = m_now;
    VitalSample sample;
    sample.takenAt = m_now;
    sample.hasFingerContact = m_hasFingerContact && m_lastOptical;
    // sem temperatura (DS18B20 ausente) a amostra vale pelos sinais ópticos; 0 = sem leitura
    sample.vitals.temperature = m_temperature.value_or(0.0);
    if(m_lastOptical)
    {
        sample.vitals.heartRate = m_lastOptical->heartRate;
        sample.vitals.oxygenSaturation = m_lastOptical->oxygenSaturation;
        sample.vitals.respiratoryRate = m_lastOptical->respiratoryRate;
    }
    m_session.vitalHistory.append(sample);
}

void MonitorController::addEvent(const QString& text, EventKind kind)
{
    if(isShiftActive())
    {
        m_session.events.append({m_now, text, kind});
    }
}

bool MonitorController::isShiftActive() const
{
    return m_session.shiftStartedAt.has_value();
}

void MonitorController::clearPatientRecords()
{
    m_session.pressureHistory.clear();
    m_session.oxygen.reset();
    m_session.events.clear();
    m_session.scoreLog.clear();
    m_session.vitalHistory.clear();
    m_session.prediction = PredictionState{};
}

void MonitorController::showToast(const QString& text, qint64 durationMilliseconds)
{
    m_toastText = text;
    m_toastUntil = m_now + durationMilliseconds;
}

ResponseLevel MonitorController::currentLevel() const
{
    return News2::responseLevel(m_snapshot);
}

bool MonitorController::isMuted() const
{
    return m_now < m_muteUntil;
}

bool MonitorController::isTechnicalAlarmActive() const
{
    return !m_hasFingerContact && m_hasEverHadContact;
}

int MonitorController::effectiveIntervalMinutes() const
{
    int intervalMinutes = News2::monitoringIntervalMinutes(m_snapshot);
    if(m_snapshot.isComplete)
    {
        return intervalMinutes;
    }
    const auto lastComplete = std::find_if(
        m_session.scoreLog.crbegin(), m_session.scoreLog.crend(),
        [](const ScoreLogEntry& entry) { return entry.isComplete; }
    );
    const int fallback = lastComplete != m_session.scoreLog.crend()
        ? lastComplete->intervalMinutes
        : 240;
    return std::min(intervalMinutes, fallback);
}

std::optional<PressureRecord> MonitorController::lastPressure() const
{
    if(m_session.pressureHistory.isEmpty())
    {
        return std::nullopt;
    }
    return m_session.pressureHistory.last();
}

void MonitorController::toggleMute()
{
    if(currentLevel() != ResponseLevel::None)
    {
        return;
    }
    if(isMuted())
    {
        m_muteUntil = 0;
        addEvent(QStringLiteral("Som reativado manualmente"), EventKind::Staff);
    }
    else
    {
        const qint64 duration = AlarmPolicy::muteDurationMilliseconds(currentLevel());
        m_muteUntil = m_now + duration;
        addEvent(
            QStringLiteral("Som silenciado por %1 min").arg(duration / Minute),
            EventKind::Staff
        );
    }
    refreshViews();
}

void MonitorController::toggleNightMode()
{
    m_isNightMode = !m_isNightMode;
    m_wakeUntil = 0;
    refreshViews();
}

void MonitorController::wake()
{
    if(m_isNightMode && m_now > m_wakeUntil)
    {
        m_wakeUntil = m_now + WakeDuration;
        refreshViews();
    }
}

void MonitorController::recordManualEntry(int systolic, bool isOnSupplementalOxygen)
{
    m_session.pressureHistory.append({systolic, m_now});
    m_session.oxygen = OxygenRecord{isOnSupplementalOxygen, m_now};
    const QString oxygenText = oxygenDescription(isOnSupplementalOxygen);
    addEvent(
        QStringLiteral("PA %1 registrada · %2").arg(systolic).arg(oxygenText),
        EventKind::Registration
    );
    showToast(
        QStringLiteral("PA %1 mmHg · %2 · gravado às %3")
            .arg(systolic).arg(oxygenText, TimeFormat::hoursMinutes(m_now)),
        5000
    );
    takeSnapshot();
    refreshViews();
}

int MonitorController::projectedTotal(int systolic, int oxygenChoice) const
{
    News2::Inputs inputs = currentInputs();
    inputs.systolicPressure = systolic;
    if(oxygenChoice >= 0)
    {
        inputs.isOnSupplementalOxygen = oxygenChoice == 1;
    }
    else if(!inputs.isOnSupplementalOxygen)
    {
        inputs.isOnSupplementalOxygen = false;
    }
    return News2::evaluate(inputs, m_now).total;
}

int MonitorController::pressureScore(int systolic) const
{
    return News2::scoreSystolicPressure(systolic);
}

void MonitorController::startShift()
{
    if(isShiftActive())
    {
        return;
    }
    clearPatientRecords();
    m_session.shiftStartedAt = m_now;
    m_session.prediction.lastRunAt = m_now;
    m_lastSampleAt = m_now;
    addEvent(QStringLiteral("Plantão iniciado"), EventKind::Staff);
    recordVitalSample();
    takeSnapshot();
    showToast(
        QStringLiteral("Plantão iniciado às %1").arg(TimeFormat::hoursMinutes(m_now)),
        4000
    );
    refreshViews();
}

void MonitorController::finishShift()
{
    if(!isShiftActive())
    {
        return;
    }
    clearPatientRecords();
    m_session.shiftStartedAt.reset();
    m_alarmSince.reset();
    takeSnapshot();
    showToast(QStringLiteral("Plantão finalizado · registros apagados"), 4000);
    refreshViews();
}

int MonitorController::scoreSlotValue(qint64 slotStart) const
{
    const qint64 slotEnd = slotStart + ScoreSlot;
    if(!isShiftActive() || slotEnd <= *m_session.shiftStartedAt || slotStart > m_now)
    {
        return UnknownScore;
    }
    int highest = UnknownScore;
    for(const ScoreLogEntry& entry : m_session.scoreLog)
    {
        if(entry.at >= slotStart && entry.at < slotEnd)
        {
            highest = std::max(highest, entry.total);
        }
    }
    if(highest == UnknownScore && m_now < slotEnd)
    {
        return m_snapshot.total;
    }
    return highest;
}

qint64 MonitorController::firstScoreSlot(int slotCount) const
{
    const qint64 currentSlot = (m_now / ScoreSlot) * ScoreSlot;
    if(!isShiftActive())
    {
        return currentSlot - (slotCount - 1) * ScoreSlot;
    }
    const qint64 shiftSlot = (*m_session.shiftStartedAt / ScoreSlot) * ScoreSlot;
    return std::max(shiftSlot, currentSlot - (slotCount - 1) * ScoreSlot);
}

QVariantList MonitorController::recentScoreBars(int slotCount) const
{
    const qint64 start = firstScoreSlot(slotCount);
    QVariantList bars;
    for(int index = 0; index < slotCount; ++index)
    {
        bars.append(scoreSlotValue(start + index * ScoreSlot));
    }
    return bars;
}

QVariantMap MonitorController::handoffSummary() const
{
    if(!isShiftActive())
    {
        return {
            {"isActive", false},
            {"bars", QVariantList{}},
            {"ticks", QVariantList{}},
            {"events", QVariantList{}}
        };
    }

    const qint64 start = firstScoreSlot(HandoffSlotCount);
    const qint64 currentSlot = (m_now / ScoreSlot) * ScoreSlot;
    QVariantList bars;
    for(int index = 0; index < HandoffSlotCount; ++index)
    {
        const qint64 slotStart = start + index * ScoreSlot;
        bars.append(QVariantMap{
            {"value", scoreSlotValue(slotStart)},
            {"isLabelVisible", index % 2 == 1 || slotStart == currentSlot}
        });
    }

    QVariantList ticks;
    for(int index = 0; index < 7; ++index)
    {
        ticks.append(TimeFormat::hoursMinutes(start + index * 4 * ScoreSlot));
    }

    int highest = m_snapshot.total;
    for(const ScoreLogEntry& entry : m_session.scoreLog)
    {
        highest = std::max(highest, entry.total);
    }
    const auto alarmCount = std::count_if(
        m_session.events.cbegin(), m_session.events.cend(),
        [](const ClinicalEvent& event) { return event.kind == EventKind::Alarm; }
    );

    QVariantList events;
    const int firstShown = std::max(0, int(m_session.events.size()) - 8);
    for(int index = firstShown; index < m_session.events.size(); ++index)
    {
        const ClinicalEvent& event = m_session.events[index];
        events.append(QVariantMap{
            {"time", TimeFormat::hoursMinutes(event.at)},
            {"text", event.text},
            {"kind", eventKindName(event.kind)}
        });
    }

    const qint64 startedAt = *m_session.shiftStartedAt;
    return {
        {"isActive", true},
        {"rangeText", QStringLiteral("início %1 (%2)")
             .arg(TimeFormat::hoursMinutes(startedAt), TimeFormat::duration(m_now - startedAt))},
        {"statsText", QStringLiteral("NEWS2 máx. %1 · %2 alarme(s)")
             .arg(highest).arg(alarmCount)},
        {"bars", bars},
        {"ticks", ticks},
        {"events", events}
    };
}

QVariantMap MonitorController::trends(const QString& range) const
{
    if(!isShiftActive())
    {
        return {{"rows", QVariantList{}}, {"ticks", QVariantList{}}, {"rangeText", QString()}};
    }
    qint64 start = *m_session.shiftStartedAt;
    if(range == QLatin1String("24"))
    {
        start = std::max(start, m_now - 24 * Hour);
    }
    else if(range == QLatin1String("12"))
    {
        start = std::max(start, m_now - 12 * Hour);
    }
    const qint64 span = std::max(Minute, m_now - start);
    const qint64 step = span > 30 * Hour ? 30 * Minute : span > 13 * Hour ? 15 * Minute
                                                                          : 10 * Minute;
    const auto timeToX = [start, span](qint64 at) { return double(at - start) / span * 1000.0; };
    const auto valueToY = [](double value, double low, double high) {
        return std::clamp(100.0 - (value - low) / (high - low) * 100.0, 0.0, 100.0);
    };

    struct RowDefinition
    {
        QString key;
        QString name;
        QString unit;
        double low;
        double high;
        double normalLow;
        double normalHigh;
    };
    const QList<RowDefinition> definitions = {
        {"hr", "FC", "bpm", 40, 140, 51, 90},
        {"spo2", "SpO₂", "%", 84, 100, m_session.useOxygenScale2 ? 88.0 : 96.0, 100},
        {"rr", "FR", "irpm", 4, 36, 12, 20},
        {"temp", "Temp", "°C", 34.5, 40, 36.1, 38},
        {"pa", "PA", "mmHg", 80, 200, 111, 200}
    };

    const auto sampleValue = [](const VitalSample& sample, const QString& key) -> double {
        if(key == QLatin1String("hr"))
        {
            return sample.vitals.heartRate;
        }
        if(key == QLatin1String("spo2"))
        {
            return sample.vitals.oxygenSaturation;
        }
        if(key == QLatin1String("rr"))
        {
            return sample.vitals.respiratoryRate;
        }
        return sample.vitals.temperature;
    };
    const auto formatValue = [](const QString& key, double value) {
        return key == QLatin1String("temp") ? TimeFormat::decimal(value)
                                            : QString::number(std::lround(value));
    };

    QVariantList rows;
    for(const RowDefinition& definition : definitions)
    {
        QVariantList points;
        QList<double> values;
        const bool isPressure = definition.key == QLatin1String("pa");
        const bool isOptical = !isPressure && definition.key != QLatin1String("temp");

        if(isPressure)
        {
            for(const PressureRecord& record : m_session.pressureHistory)
            {
                if(record.recordedAt < start || record.recordedAt > m_now)
                {
                    continue;
                }
                values.append(record.systolic);
                points.append(QPointF(
                    timeToX(record.recordedAt),
                    valueToY(record.systolic, definition.low, definition.high)
                ));
            }
        }
        else
        {
            qint64 nextAt = start;
            for(const VitalSample& sample : m_session.vitalHistory)
            {
                if(sample.takenAt < nextAt || (isOptical && !sample.hasFingerContact))
                {
                    continue;
                }
                const double value = sampleValue(sample, definition.key);
                if(value <= 0.0)
                {
                    continue;
                }
                values.append(value);
                points.append(QPointF(
                    timeToX(sample.takenAt),
                    valueToY(value, definition.low, definition.high)
                ));
                nextAt = sample.takenAt + step;
            }
        }

        QString nowText = QStringLiteral("—");
        if(isPressure)
        {
            if(const auto record = lastPressure())
            {
                nowText = QString::number(record->systolic);
            }
        }
        else if(definition.key == QLatin1String("temp") && m_temperature)
        {
            nowText = TimeFormat::decimal(*m_temperature);
            points.append(QPointF(1000.0, valueToY(*m_temperature, definition.low,
                                                   definition.high)));
            values.append(*m_temperature);
        }
        else if(isOptical && m_hasFingerContact && m_lastOptical)
        {
            const VitalSample current{
                m_now,
                {m_lastOptical->heartRate, m_lastOptical->oxygenSaturation,
                 m_lastOptical->respiratoryRate, 0.0},
                true
            };
            const double value = sampleValue(current, definition.key);
            if(value > 0.0)
            {
                nowText = formatValue(definition.key, value);
                points.append(QPointF(1000.0, valueToY(value, definition.low, definition.high)));
                values.append(value);
            }
        }

        QString rangeText = QStringLiteral("sem dados");
        if(!values.isEmpty())
        {
            const auto [lowest, highest] = std::minmax_element(values.cbegin(), values.cend());
            rangeText = QStringLiteral("mín %1 · máx %2")
                .arg(formatValue(definition.key, *lowest), formatValue(definition.key, *highest));
            if(isPressure)
            {
                rangeText.prepend(QStringLiteral("%1 registros · ").arg(values.size()));
            }
        }

        rows.append(QVariantMap{
            {"key", definition.key},
            {"name", definition.name},
            {"unit", definition.unit},
            {"tag", isPressure ? "MANUAL" : "MEDIDO"},
            {"nowText", nowText},
            {"rangeText", rangeText},
            {"points", points},
            {"isDashed", isPressure},
            {"hasDots", isPressure},
            {"bandTop", valueToY(definition.normalHigh, definition.low, definition.high)},
            {"bandBottom", valueToY(definition.normalLow, definition.low, definition.high)},
            {"axisTop", formatValue(definition.key, definition.high)},
            {"axisBottom", formatValue(definition.key, definition.low)}
        });
    }

    const auto tickText = [span](qint64 at) {
        return span > 24 * Hour ? TimeFormat::dayMonthHoursMinutes(at)
                                : TimeFormat::hoursMinutes(at);
    };
    QVariantList ticks;
    for(int index = 0; index < 5; ++index)
    {
        ticks.append(tickText(start + span * index / 4));
    }

    return {
        {"rows", rows},
        {"ticks", ticks},
        {"rangeText", QStringLiteral("de %1 a %2 (%3)")
             .arg(tickText(start), TimeFormat::hoursMinutes(m_now), TimeFormat::duration(span))}
    };
}

void MonitorController::refreshViews()
{
    m_patientView = {
        {"bed", m_session.patient.bed},
        {"isShiftActive", isShiftActive()},
        {"shiftSince", isShiftActive()
             ? QStringLiteral("desde %1").arg(TimeFormat::hoursMinutes(*m_session.shiftStartedAt))
             : QStringLiteral("sem plantão")}
    };

    refreshVitalsView();
    refreshScoreView();
    refreshPressureView();
    refreshPredictionView();
    refreshAlarmView();
    refreshDisplayView();
    applyAlarmIndicator();

    ++m_revision;
    emit updated();
}

void MonitorController::refreshVitalsView()
{
    const bool hasOptical = m_hasFingerContact && m_lastOptical.has_value();

    const auto lastText = [this](int value) {
        if(value <= 0)
        {
            return QString();
        }
        return QStringLiteral("último %1 às %2")
            .arg(value).arg(TimeFormat::hoursMinutes(m_lastOptical ? m_lastOptical->at : 0));
    };
    const auto sinceText = [](const TrendTracker& tracker) {
        return tracker.direction() == TrendTracker::Direction::Steady
            ? QString()
            : TimeFormat::hoursMinutes(tracker.since());
    };

    m_vitalsView = {
        {"hasFingerContact", hasOptical},
        {"isWaitingForFinger", !m_hasFingerContact && !m_hasEverHadContact},
        {"heartRate", m_lastOptical ? m_lastOptical->heartRate : 0},
        {"oxygenSaturation", m_lastOptical ? m_lastOptical->oxygenSaturation : 0},
        {"respiratoryRate", m_lastOptical ? m_lastOptical->respiratoryRate : 0},
        {"temperatureText", m_temperature ? TimeFormat::decimal(*m_temperature)
                                          : QStringLiteral("—")},
        {"heartRateScore", scoreOrUnknown(m_snapshot.subscore(Parameter::HeartRate))},
        {"oxygenSaturationScore",
         scoreOrUnknown(m_snapshot.subscore(Parameter::OxygenSaturation))},
        {"respiratoryRateScore", scoreOrUnknown(m_snapshot.subscore(Parameter::RespiratoryRate))},
        {"temperatureScore", scoreOrUnknown(m_snapshot.subscore(Parameter::Temperature))},
        {"heartRateTrend", int(m_heartRateTrend.direction())},
        {"oxygenSaturationTrend", int(m_oxygenSaturationTrend.direction())},
        {"respiratoryRateTrend", int(m_respiratoryRateTrend.direction())},
        {"heartRateTrendSince", sinceText(m_heartRateTrend)},
        {"oxygenSaturationTrendSince", sinceText(m_oxygenSaturationTrend)},
        {"respiratoryRateTrendSince", sinceText(m_respiratoryRateTrend)},
        {"oxygenSaturationUnit", m_session.useOxygenScale2 ? QStringLiteral("% · Esc. 2")
                                                           : QStringLiteral("%")},
        {"heartRateLast", lastText(m_lastOptical ? m_lastOptical->heartRate : 0)},
        {"oxygenSaturationLast", lastText(m_lastOptical ? m_lastOptical->oxygenSaturation : 0)},
        {"respiratoryRateLast", lastText(m_lastOptical ? m_lastOptical->respiratoryRate : 0)}
    };
}

void MonitorController::refreshScoreView()
{
    const ResponseLevel level = currentLevel();
    const int total = m_snapshot.total;
    const int bandIndex = total >= 7 ? 3 : total >= 5 ? 2 : total >= 1 ? 1 : 0;
    const bool isSoundOn = level != ResponseLevel::None && !isMuted();

    QStringList missing;
    for(Parameter parameter : News2::AllParameters)
    {
        if(!m_snapshot.subscore(parameter))
        {
            missing << shortLabel(parameter);
        }
    }

    QString scoreText = QStringLiteral("—");
    if(m_snapshot.isComplete)
    {
        scoreText = QString::number(total);
    }
    else if(total >= 5)
    {
        scoreText = QStringLiteral("≥%1").arg(total);
    }

    QString title;
    QString action;
    QString titleTone = QString::number(bandIndex);
    if(!m_snapshot.isComplete && total < 5)
    {
        title = QStringLiteral("INCOMPLETO");
        action = QStringLiteral("Faltam %1. Parcial: %2").arg(missing.join(", ")).arg(total);
        titleTone = QStringLiteral("text");
    }
    else if(level == ResponseLevel::Emergency)
    {
        title = QStringLiteral("EMERGÊNCIA · ≥ 7");
        action = QStringLiteral("Equipe de cuidado crítico · considerar UTI");
    }
    else if(level == ResponseLevel::Urgent)
    {
        title = QStringLiteral("URGENTE · 5–6");
        action = QStringLiteral("Avaliação médica urgente · aferir a cada 1 h");
    }
    else if(level == ResponseLevel::SingleParameter)
    {
        title = QStringLiteral("3 EM %1 · parâmetro único")
            .arg(shortLabel(*m_snapshot.parameterScoringThree));
        action = QStringLiteral("Enfermeiro informa equipe médica · aferir a cada 1 h");
        titleTone = QStringLiteral("2");
    }
    else if(total >= 1)
    {
        title = QStringLiteral("BAIXO · 1–4");
        action = QStringLiteral("Informar o enfermeiro · aferir a cada 4–6 h");
    }
    else
    {
        title = QStringLiteral("BAIXO · 0");
        action = QStringLiteral("Rotina · aferir a cada 12 h");
    }

    const qint64 secondsToNext = std::max<qint64>(
        0, (SnapshotPeriod - (m_now - m_snapshot.takenAt) + 999) / 1000
    );

    m_scoreView = {
        {"total", total},
        {"isComplete", m_snapshot.isComplete},
        {"isFilled", m_snapshot.isComplete || total >= 5},
        {"text", scoreText},
        {"bandIndex", bandIndex},
        {"level", toInt(level)},
        {"title", title},
        {"titleTone", titleTone},
        {"action", action},
        {"isSoundOn", isSoundOn},
        {"calculationText", QStringLiteral("às %1 · próx. %2 s")
             .arg(TimeFormat::hoursMinutesSeconds(m_snapshot.takenAt)).arg(secondsToNext)}
    };

    m_ribbonView.clear();
    for(Parameter parameter : News2::AllParameters)
    {
        m_ribbonView.append(QVariantMap{
            {"label", shortLabel(parameter)},
            {"score", scoreOrUnknown(m_snapshot.subscore(parameter))},
            {"provenance", provenanceOf(parameter)}
        });
    }
}

void MonitorController::refreshPressureView()
{
    const int intervalMinutes = effectiveIntervalMinutes();
    const auto record = lastPressure();

    QVariantMap view = {
        {"hasRecord", record.has_value()},
        {"valueText", QStringLiteral("—")},
        {"opacity", 1.0},
        {"isStruck", false},
        {"ageFraction", 0.0},
        {"ageTone", "orange"},
        {"ageText", QStringLiteral("sem registro · use Inserir PA")},
        {"border", "normal"},
        {"dueShort", QStringLiteral("sem registro")},
        {"dueFull", QStringLiteral("sem registro — registrar agora")},
        {"isOverdue", true},
        {"previousText", QStringLiteral("sem registro anterior")},
        {"previousValue", 0}
    };

    if(record)
    {
        const qint64 age = m_now - record->recordedAt;
        const qint64 interval = intervalMinutes * Minute;
        const double fraction = interval > 0 ? double(age) / interval : 1.0;
        const bool isOverdue = fraction >= 1.0;
        const QString tone = isOverdue ? "red" : fraction >= 0.75 ? "yellow" : "dim";

        view["valueText"] = QString::number(record->systolic);
        view["opacity"] = isOverdue ? 0.5 : 1.0 - 0.35 * std::min(1.0, fraction);
        view["isStruck"] = isOverdue;
        view["ageFraction"] = clampedFraction(fraction);
        view["ageTone"] = tone;
        view["isOverdue"] = isOverdue;
        view["border"] = isOverdue ? "overdue" : "normal";
        view["previousText"] = QStringLiteral("anterior %1 às %2")
            .arg(record->systolic).arg(TimeFormat::hoursMinutes(record->recordedAt));
        view["previousValue"] = record->systolic;

        if(interval == 0)
        {
            view["ageText"] = QStringLiteral("%1 · reaferir já").arg(TimeFormat::duration(age));
            view["dueShort"] = QStringLiteral("aferir agora");
            view["dueFull"] = QStringLiteral("registrada há %1 — reaferir agora "
                                             "(monitoramento contínuo)")
                                  .arg(TimeFormat::duration(age));
        }
        else if(isOverdue)
        {
            const QString overdueFor = TimeFormat::duration(age - interval);
            view["ageText"] = QStringLiteral("VENCIDA há %1").arg(overdueFor);
            view["dueShort"] = QStringLiteral("vencida há %1").arg(overdueFor);
            view["dueFull"] = QStringLiteral("vencida desde %1 — reaferir")
                                  .arg(TimeFormat::hoursMinutes(record->recordedAt + interval));
        }
        else
        {
            const QString intervalText = intervalMinutes >= 240
                ? QStringLiteral("4–6 h")
                : QStringLiteral("%1 h").arg(intervalMinutes / 60);
            view["ageText"] = QStringLiteral("%1 · vence %2")
                                  .arg(TimeFormat::duration(age),
                                       TimeFormat::duration(interval - age));
            view["dueShort"] = QStringLiteral("vence em %1")
                                   .arg(TimeFormat::duration(interval - age));
            view["dueFull"] = QStringLiteral("vence às %1 (intervalo %2)")
                                  .arg(TimeFormat::hoursMinutes(record->recordedAt + interval),
                                       intervalText);
        }
        if(m_snapshot.subscore(Parameter::SystolicPressure) == 3)
        {
            view["border"] = "critical";
        }
    }

    view["oxygenText"] = m_session.oxygen
        ? (m_session.oxygen->isOnSupplementalOxygen ? QStringLiteral("O₂ suplementar")
                                                    : QStringLiteral("Ar ambiente"))
        : QStringLiteral("O₂ não registrado");
    view["oxygenChoice"] = m_session.oxygen
        ? (m_session.oxygen->isOnSupplementalOxygen ? 1 : 0)
        : -1;
    view["score"] = scoreOrUnknown(m_snapshot.subscore(Parameter::SystolicPressure));
    view["oxygenScore"] = scoreOrUnknown(m_snapshot.subscore(Parameter::SupplementalOxygen));
    m_pressureView = view;
}

void MonitorController::refreshPredictionView()
{
    if(!isShiftActive())
    {
        m_predictionView = {
            {"state", "idle"},
            {"headline", QStringLiteral("SEM PLANTÃO")},
            {"isSlowBlinkOn", true},
            {"title", QStringLiteral("Predição 6 h")},
            {"nextRunText", QStringLiteral("Inicie o plantão para coletar dados")},
            {"progress", 0.0},
            {"lastResultText", QStringLiteral("Nenhuma execução ainda")},
            {"timing", QStringLiteral("sem plantão · nenhum dado coletado")}
        };
        return;
    }

    const PredictionState& prediction = m_session.prediction;
    const qint64 shiftStartedAt = *m_session.shiftStartedAt;
    const qint64 windowEnd = shiftStartedAt + PredictionWindow;
    const bool isCollecting = m_now < windowEnd || !prediction.probability;
    const bool isPositive = prediction.probability.value_or(0) >= PredictionPositiveThreshold;
    const qint64 runsUntilWindow = std::max<qint64>(
        1, (windowEnd - prediction.lastRunAt + PredictionPeriod - 1) / PredictionPeriod
    );
    const qint64 firstResultAt = prediction.lastRunAt + runsUntilWindow * PredictionPeriod;
    const qint64 sinceRun = m_now - prediction.lastRunAt;
    const QString lastRun = TimeFormat::hoursMinutes(prediction.lastRunAt);
    const QString nextRun = TimeFormat::hoursMinutes(prediction.lastRunAt + PredictionPeriod);

    QString state;
    QString headline;
    if(isCollecting)
    {
        state = QStringLiteral("collecting");
        headline = QStringLiteral("ESTIMANDO…");
    }
    else if(!m_hasFingerContact)
    {
        state = QStringLiteral("paused");
        headline = QStringLiteral("PAUSADA");
    }
    else if(m_snapshot.total >= 5)
    {
        state = QStringLiteral("reached");
        headline = QStringLiteral("LIMIAR ATINGIDO");
    }
    else if(isPositive)
    {
        state = QStringLiteral("risk");
        headline = QStringLiteral("RISCO DE PIORA");
    }
    else
    {
        state = QStringLiteral("clear");
        headline = QStringLiteral("SEM ALERTA");
    }

    QString lastResult;
    if(prediction.lastRunAt <= shiftStartedAt)
    {
        lastResult = QStringLiteral("Nenhuma execução ainda");
    }
    else
    {
        lastResult = QStringLiteral("Última execução %1 · %2")
            .arg(lastRun,
                 isCollecting ? QStringLiteral("dados insuficientes")
                              : isPositive ? QStringLiteral("risco de piora")
                                           : QStringLiteral("sem alerta"));
    }

    m_predictionView = {
        {"state", state},
        {"headline", headline},
        {"isSlowBlinkOn", (m_now / 1500) % 2 == 0},
        {"title", isCollecting
             ? QStringLiteral("Predição em %1").arg(TimeFormat::duration(firstResultAt - m_now))
             : QStringLiteral("Predição 6 h")},
        {"nextRunText", QStringLiteral("Próxima execução em %1")
             .arg(TimeFormat::duration(PredictionPeriod - sinceRun))},
        {"progress", clampedFraction(double(sinceRun) / PredictionPeriod)},
        {"lastResultText", lastResult},
        {"timing", isCollecting
             ? QStringLiteral("coletando dados · resultado às %1")
                   .arg(TimeFormat::hoursMinutes(firstResultAt))
             : QStringLiteral("última execução %1 · próxima %2").arg(lastRun, nextRun)}
    };
}

void MonitorController::refreshAlarmView()
{
    const ResponseLevel level = currentLevel();
    const bool isClinical = level != ResponseLevel::None;
    const bool isTechnical = isTechnicalAlarmActive();
    const bool isMutedNow = isMuted();
    const bool isClinicalSoundOn = isClinical && !isMutedNow;
    const bool isTechnicalSoundOn = !isClinical && isTechnical && !isMutedNow;

    QString title;
    QString subtitle;
    QString tone;
    if(isClinical)
    {
        tone = level == ResponseLevel::Emergency ? QStringLiteral("red") : QStringLiteral("orange");
        switch(level)
        {
            case ResponseLevel::Emergency:
                title = QStringLiteral("NEWS2 %1 · EMERGÊNCIA").arg(m_snapshot.total);
                subtitle = QStringLiteral("Chamar equipe de cuidado crítico · considerar UTI");
                break;
            case ResponseLevel::Urgent:
                title = QStringLiteral("NEWS2 %1 · RESPOSTA URGENTE").arg(m_snapshot.total);
                subtitle = QStringLiteral("Avaliação médica urgente · aferir a cada 1 h");
                break;
            default:
                title = QStringLiteral("%1 em 3 pontos · parâmetro único")
                    .arg(shortLabel(*m_snapshot.parameterScoringThree));
                subtitle = QStringLiteral("Enfermeiro informa a equipe médica");
                break;
        }
        if(isTechnical)
        {
            subtitle = QStringLiteral("Sensor de dedo sem contato · escore parcial");
        }
    }

    m_alarmView = {
        {"isVisible", isClinical},
        {"isClinical", isClinical},
        {"level", toInt(level)},
        {"tone", tone},
        {"title", title},
        {"subtitle", subtitle},
        {"isMuted", isMutedNow},
        {"isSounding", isClinicalSoundOn || isTechnicalSoundOn},
        {"isTechnicalSoundOn", isTechnicalSoundOn},
        {"soundTone", level == ResponseLevel::Emergency ? "red" : "orange"},
        {"muteText", isMutedNow
             ? QStringLiteral("volta em %1").arg(TimeFormat::minutesSeconds(m_muteUntil - m_now))
             : QStringLiteral("Silenciar")},
        {"frameTone", level == ResponseLevel::Emergency ? "red"
                      : level == ResponseLevel::Urgent  ? "orange"
                                                        : ""}
    };
}

void MonitorController::refreshDisplayView()
{
    const bool isToastVisible = m_now < m_toastUntil;
    const bool isNightDimmed = m_isNightMode
        && currentLevel() == ResponseLevel::None
        && m_now > m_wakeUntil;
    m_displayView = {
        {"isNightMode", m_isNightMode},
        {"isNightDimmed", isNightDimmed},
        {"toastText", isToastVisible ? m_toastText : QString()}
    };
}

void MonitorController::applyAlarmIndicator()
{
    const ResponseLevel level = currentLevel();
    const bool isMutedNow = isMuted();

    if(level != ResponseLevel::None)
    {
        const bool isEmergency = level == ResponseLevel::Emergency;
        if(isMutedNow)
        {
            m_alarmIndicator.apply(
                isEmergency ? LedPattern::SteadyRed : LedPattern::SteadyOrange,
                BuzzerPattern::Silent
            );
            return;
        }
        m_alarmIndicator.apply(
            isEmergency ? LedPattern::BlinkingRed : LedPattern::BlinkingOrange,
            isEmergency ? BuzzerPattern::HighPriority : BuzzerPattern::MediumPriority
        );
        return;
    }
    if(isTechnicalAlarmActive())
    {
        m_alarmIndicator.apply(
            LedPattern::SteadyWhite,
            isMutedNow ? BuzzerPattern::Silent : BuzzerPattern::LowPriority
        );
        return;
    }
    m_alarmIndicator.apply(
        m_isNightMode ? LedPattern::Off : LedPattern::SteadyGreen,
        BuzzerPattern::Silent
    );
}
