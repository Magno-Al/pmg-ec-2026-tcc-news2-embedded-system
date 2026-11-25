import QtQuick
import MonitorNews2

// Curva de um parâmetro do PPG com valor, tendência e subescore à esquerda, na mesma linha.
Rectangle {
    id: row

    property string key
    property string name
    property string unit
    property string valueText
    property string lastReadingText
    property int score: -1
    property int trendDirection: 0
    property string trendSince
    property real rate: 60
    property int shape: WaveformItem.Pulse
    property int liveChannel: WaveformItem.None

    readonly property bool isOnline: Monitor.vitals.hasFingerContact

    color: Theme.waveBackground
    border.width: score === 3 ? 3 : 1
    border.color: score === 3 ? Theme.score3 : Theme.line
    clip: true

    VitalValuePanel {
        id: valuePanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: row.border.width
        width: 200 - row.border.width
        key: row.key
        name: row.name
        unit: row.unit
        valueText: row.valueText
        score: row.score
        trendDirection: row.trendDirection
        trendSince: row.trendSince
    }

    Item {
        anchors.left: valuePanel.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 1
        anchors.leftMargin: 0

        WaveformItem {
            anchors.fill: parent
            color: Theme.parameterColor(row.key)
            idleColor: Theme.strongLine
            shape: row.shape
            rate: row.rate
            isActive: row.isOnline
            liveChannel: row.liveChannel
        }

        UiText {
            x: 10
            y: 5
            text: row.name
            font.bold: true
            color: valuePanel.parameterColor
        }

        NoSignalOverlay {
            anchors.fill: parent
            lastReadingText: row.lastReadingText
        }
    }
}
