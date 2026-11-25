import QtQuick
import MonitorNews2

// Valor de um parâmetro do PPG: nome, unidade e procedência em cima; valor, tendência e
// subescore embaixo. Fica à esquerda da curva (WaveRow, PulseOximetryRow).
Rectangle {
    id: panel

    property string key
    property string name
    property string unit
    property string valueText
    property int score: -1
    property int trendDirection: 0
    property string trendSince

    readonly property bool isOnline: Monitor.vitals.hasFingerContact
    readonly property color parameterColor: isOnline ? Theme.parameterColor(key) : Theme.dim
    readonly property bool hasTrend: isOnline && trendDirection !== 0

    color: Theme.panel

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.divider
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 2

        Item {
            width: parent.width
            height: 24

            UiText {
                id: headerName
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: panel.name
                font.pixelSize: 20
                font.bold: true
                color: panel.parameterColor
            }

            UiText {
                anchors.left: headerName.right
                anchors.leftMargin: 6
                anchors.baseline: headerName.baseline
                text: panel.unit
                color: Theme.dim
            }

            ProvenanceMark {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                markColor: panel.parameterColor
            }
        }

        Item {
            width: parent.width
            height: Math.max(valueText.height, badge.height)

            UiText {
                id: valueText
                anchors.left: parent.left
                anchors.right: trend.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: panel.isOnline ? panel.valueText : "—"
                font.pixelSize: 54
                font.weight: Font.DemiBold
                font.letterSpacing: -1.5
                lineHeightMode: Text.FixedHeight
                lineHeight: font.pixelSize
                color: panel.parameterColor
            }

            // Tendência: seta pisca junto com a leitura; o horário marca o início da direção.
            Column {
                id: trend
                anchors.right: badge.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 38
                spacing: 2

                UiText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: panel.hasTrend ? (panel.trendDirection > 0 ? "▲" : "▼") : ""
                    font.pixelSize: 20
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 20
                    opacity: Monitor.blinkPhase ? 1 : 0.25
                }

                UiText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: panel.hasTrend ? panel.trendSince : ""
                    font.pixelSize: 13
                    font.letterSpacing: -0.3
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 13
                    color: Theme.dim
                }
            }

            ScoreBadge {
                id: badge
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 38
                height: width
                fontSize: 22
                score: panel.isOnline ? panel.score : -1
            }
        }
    }
}
