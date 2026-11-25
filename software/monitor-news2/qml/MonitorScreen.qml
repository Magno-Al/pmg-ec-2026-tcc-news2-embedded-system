import QtQuick
import MonitorNews2

// Tela-mãe: onde o equipamento fica 99% do tempo. Não há rolagem — tudo cabe em 1024 × 600.
Item {
    id: screen

    // altura de uma linha de curva; o bloco de FC e SpO₂ ocupa duas
    readonly property int waveRowHeight: 88

    signal registerRequested()
    signal trendsRequested()
    signal handoffRequested()
    signal helpRequested()

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

            PulseOximetryRow {
                width: parent.width
                height: 2 * screen.waveRowHeight + leftColumn.spacing
            }

            WaveRow {
                width: parent.width
                height: screen.waveRowHeight
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
            }

            Row {
                width: parent.width
                height: leftColumn.height - 3 * screen.waveRowHeight - 3 * leftColumn.spacing
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
