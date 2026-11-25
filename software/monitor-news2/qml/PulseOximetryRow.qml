import QtQuick
import MonitorNews2

// FC e SpO₂ saem da mesma onda de pulso do oxímetro: os dois valores ficam empilhados à
// esquerda e a pletismografia ocupa uma curva só, com a altura das duas linhas.
Rectangle {
    id: row

    readonly property bool isOnline: Monitor.vitals.hasFingerContact
    readonly property bool hasCriticalScore: Monitor.vitals.heartRateScore === 3
                                             || Monitor.vitals.oxygenSaturationScore === 3

    color: Theme.waveBackground
    border.width: hasCriticalScore ? 3 : 1
    border.color: hasCriticalScore ? Theme.score3 : Theme.line
    clip: true

    Column {
        id: valuePanels
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: row.border.width
        width: 200 - row.border.width

        VitalValuePanel {
            width: parent.width
            height: (parent.height - panelDivider.height) / 2
            key: "hr"
            name: "FC"
            unit: "bpm"
            trendDirection: Monitor.vitals.heartRateTrend
            trendSince: Monitor.vitals.heartRateTrendSince
            valueText: Monitor.vitals.heartRate > 0 ? String(Monitor.vitals.heartRate) : "—"
            score: Monitor.vitals.heartRateScore
        }

        Rectangle {
            id: panelDivider
            width: parent.width
            height: 1
            color: Theme.divider
        }

        VitalValuePanel {
            width: parent.width
            height: (parent.height - panelDivider.height) / 2
            key: "spo2"
            name: "SpO₂"
            unit: Monitor.vitals.oxygenSaturationUnit
            trendDirection: Monitor.vitals.oxygenSaturationTrend
            trendSince: Monitor.vitals.oxygenSaturationTrendSince
            valueText: Monitor.vitals.oxygenSaturation > 0
                       ? String(Monitor.vitals.oxygenSaturation) : "—"
            score: Monitor.vitals.oxygenSaturationScore
        }
    }

    Item {
        anchors.left: valuePanels.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 1
        anchors.leftMargin: 0

        WaveformItem {
            anchors.fill: parent
            color: Theme.parameterColor("hr")
            idleColor: Theme.strongLine
            shape: WaveformItem.Plethysmography
            rate: Monitor.vitals.heartRate
            isActive: row.isOnline
            liveChannel: WaveformItem.Plethysmography
        }

        UiText {
            x: 10
            y: 5
            text: "PLETH"
            font.bold: true
            color: row.isOnline ? Theme.parameterColor("hr") : Theme.dim
        }

        NoSignalOverlay {
            anchors.fill: parent
            lastReadingText: Monitor.vitals.heartRateLast
        }
    }
}
