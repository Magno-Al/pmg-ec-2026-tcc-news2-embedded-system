import QtQuick
import MonitorNews2

// Tela-mãe: onde o equipamento fica 99% do tempo. Não há rolagem — tudo cabe em 1024 × 600.
Item {
    id: screen

    property string expandedKey: ""

    signal registerRequested()
    signal trendsRequested()
    signal handoffRequested()
    signal helpRequested()

    function waveHeight(key) {
        if (expandedKey === "")
            return 88
        return expandedKey === key ? 160 : 52
    }

    function toggleExpanded(key) {
        expandedKey = expandedKey === key ? "" : key
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 456
        anchors.margins: 8

        Column {
            id: leftColumn
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: scorePanel.left
            anchors.rightMargin: 8
            spacing: 8

            WaveRow {
                width: parent.width
                height: screen.waveHeight(key)
                key: "hr"
                name: "FC"
                unit: "bpm"
                trendDirection: Monitor.vitals.heartRateTrend
                trendSince: Monitor.vitals.heartRateTrendSince
                valueText: Monitor.vitals.heartRate > 0
                           ? String(Monitor.vitals.heartRate) : "—"
                lastReadingText: Monitor.vitals.heartRateLast
                score: Monitor.vitals.heartRateScore
                rate: Monitor.vitals.heartRate
                shape: WaveformItem.Pulse
                liveChannel: WaveformItem.Plethysmography
                isExpanded: screen.expandedKey === key
                isCompact: screen.expandedKey !== "" && !isExpanded
                onToggled: screen.toggleExpanded(key)
            }

            WaveRow {
                width: parent.width
                height: screen.waveHeight(key)
                key: "spo2"
                name: "SpO₂"
                unit: Monitor.vitals.oxygenSaturationUnit
                trendDirection: Monitor.vitals.oxygenSaturationTrend
                trendSince: Monitor.vitals.oxygenSaturationTrendSince
                valueText: Monitor.vitals.oxygenSaturation > 0
                           ? String(Monitor.vitals.oxygenSaturation) : "—"
                lastReadingText: Monitor.vitals.oxygenSaturationLast
                score: Monitor.vitals.oxygenSaturationScore
                rate: Monitor.vitals.heartRate
                shape: WaveformItem.Plethysmography
                liveChannel: WaveformItem.Plethysmography
                phaseOffset: 0.08
                isExpanded: screen.expandedKey === key
                isCompact: screen.expandedKey !== "" && !isExpanded
                onToggled: screen.toggleExpanded(key)
            }

            WaveRow {
                width: parent.width
                height: screen.waveHeight(key)
                key: "rr"
                name: "FR"
                unit: "irpm"
                trendDirection: Monitor.vitals.respiratoryRateTrend
                trendSince: Monitor.vitals.respiratoryRateTrendSince
                valueText: Monitor.vitals.respiratoryRate > 0
                           ? String(Monitor.vitals.respiratoryRate) : "—"
                lastReadingText: Monitor.vitals.respiratoryRateLast
                score: Monitor.vitals.respiratoryRateScore
                rate: Monitor.vitals.respiratoryRate
                shape: WaveformItem.Respiration
                liveChannel: WaveformItem.Respiration
                isExpanded: screen.expandedKey === key
                isCompact: screen.expandedKey !== "" && !isExpanded
                onToggled: screen.toggleExpanded(key)
            }

            Row {
                width: parent.width
                height: leftColumn.height - 3 * 88 - 3 * leftColumn.spacing
                spacing: 8

                TemperatureTile {
                    width: (parent.width - parent.spacing) * 0.4
                    height: parent.height
                }

                PressureTile {
                    width: (parent.width - parent.spacing) * 0.6
                    height: parent.height
                }
            }
        }

        ScorePanel {
            id: scorePanel
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 372
            onHelpRequested: screen.helpRequested()
        }
    }

    BottomBar {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onRegisterRequested: screen.registerRequested()
        onTrendsRequested: screen.trendsRequested()
        onHandoffRequested: screen.handoffRequested()
    }
}
