#pragma once

#include "core/News2Score.h"
#include "core/PatientSession.h"
#include "core/TrendTracker.h"
#include "core/VitalSigns.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

#include <optional>

class AlarmIndicator;
class DeteriorationPredictor;
class VitalSignsSource;

// View-model da tela do monitor. Recebe leituras da fonte de sinais vitais, fecha um snapshot
// do NEWS2 a cada 30 s, roda a predição de hora em hora e expõe ao QML o estado já traduzido
// em texto e tons. Os mapas são recalculados a cada tique de 500 ms (que também marca o
// piscar de 1 Hz) e notificados por um único sinal.
class MonitorController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Monitor)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY updated)
    Q_PROPERTY(QString clockText READ clockText NOTIFY updated)
    Q_PROPERTY(bool blinkPhase READ blinkPhase NOTIFY updated)
    Q_PROPERTY(QVariantMap patient READ patient NOTIFY updated)
    Q_PROPERTY(QVariantMap vitals READ vitals NOTIFY updated)
    Q_PROPERTY(QVariantMap score READ score NOTIFY updated)
    Q_PROPERTY(QVariantList ribbon READ ribbon NOTIFY updated)
    Q_PROPERTY(QVariantMap pressure READ pressure NOTIFY updated)
    Q_PROPERTY(QVariantMap prediction READ prediction NOTIFY updated)
    Q_PROPERTY(QVariantMap alarm READ alarm NOTIFY updated)
    Q_PROPERTY(QVariantMap display READ display NOTIFY updated)

public:
    MonitorController(
        PatientSession session,
        VitalSignsSource& vitalSignsSource,
        DeteriorationPredictor& predictor,
        AlarmIndicator& alarmIndicator,
        QObject* parent = nullptr
    );

    static void setInstance(MonitorController* instance);
    static MonitorController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    void start();

    int revision() const { return m_revision; }
    QString clockText() const;
    bool blinkPhase() const { return m_isBlinkOn; }
    QVariantMap patient() const { return m_patientView; }
    QVariantMap vitals() const { return m_vitalsView; }
    QVariantMap score() const { return m_scoreView; }
    QVariantList ribbon() const { return m_ribbonView; }
    QVariantMap pressure() const { return m_pressureView; }
    QVariantMap prediction() const { return m_predictionView; }
    QVariantMap alarm() const { return m_alarmView; }
    QVariantMap display() const { return m_displayView; }

    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void toggleNightMode();
    Q_INVOKABLE void wake();
    Q_INVOKABLE void recordManualEntry(int systolic, bool isOnSupplementalOxygen);
    // oxygenChoice: -1 mantém o registro atual de O2, 0 ar ambiente, 1 O2 suplementar
    Q_INVOKABLE int projectedTotal(int systolic, int oxygenChoice) const;
    Q_INVOKABLE int pressureScore(int systolic) const;
    Q_INVOKABLE void startShift();
    Q_INVOKABLE void finishShift();
    Q_INVOKABLE QVariantMap handoffSummary() const;
    Q_INVOKABLE QVariantList recentScoreBars(int slotCount) const;
    Q_INVOKABLE QVariantMap trends(const QString& range) const;

signals:
    void updated();

private:
    struct OpticalReading
    {
        int heartRate = 0;
        int oxygenSaturation = 0;
        int respiratoryRate = 0;
        qint64 at = 0;
    };

    void onPulseOximetry(int heartRate, int oxygenSaturation, int respiratoryRate);
    void onTemperature(double celsius);
    void onFingerContactChanged(bool hasFingerContact);
    void tick();

    News2::Inputs currentInputs() const;
    void takeSnapshot();
    void applySnapshot(const News2::Snapshot& snapshot);
    void runPrediction();
    void recordVitalSample();
    void addEvent(const QString& text, EventKind kind);
    bool isShiftActive() const;
    void clearPatientRecords();
    void showToast(const QString& text, qint64 durationMilliseconds);

    News2::ResponseLevel currentLevel() const;
    bool isMuted() const;
    bool isTechnicalAlarmActive() const;
    int effectiveIntervalMinutes() const;
    std::optional<PressureRecord> lastPressure() const;
    int scoreSlotValue(qint64 slotStart) const;
    qint64 firstScoreSlot(int slotCount) const;

    void refreshViews();
    void refreshVitalsView();
    void refreshScoreView();
    void refreshPressureView();
    void refreshPredictionView();
    void refreshAlarmView();
    void refreshDisplayView();
    void applyAlarmIndicator();

    static MonitorController* s_instance;

    PatientSession m_session;
    VitalSignsSource& m_vitalSignsSource;
    DeteriorationPredictor& m_predictor;
    AlarmIndicator& m_alarmIndicator;
    QTimer m_tickTimer;

    qint64 m_now = 0;
    bool m_isBlinkOn = false;
    int m_revision = 0;

    std::optional<OpticalReading> m_lastOptical;
    std::optional<double> m_temperature;
    bool m_hasFingerContact = true;
    bool m_hasEverHadContact = false;
    TrendTracker m_heartRateTrend;
    TrendTracker m_oxygenSaturationTrend;
    TrendTracker m_respiratoryRateTrend;
    qint64 m_contactLostAt = 0;
    qint64 m_lastSampleAt = 0;

    News2::Snapshot m_snapshot;
    News2::ResponseLevel m_previousLevel = News2::ResponseLevel::None;
    std::optional<qint64> m_alarmSince;
    qint64 m_muteUntil = 0;

    bool m_isNightMode = false;
    qint64 m_wakeUntil = 0;
    QString m_toastText;
    qint64 m_toastUntil = 0;

    QVariantMap m_patientView;
    QVariantMap m_vitalsView;
    QVariantMap m_scoreView;
    QVariantList m_ribbonView;
    QVariantMap m_pressureView;
    QVariantMap m_predictionView;
    QVariantMap m_alarmView;
    QVariantMap m_displayView;
};
