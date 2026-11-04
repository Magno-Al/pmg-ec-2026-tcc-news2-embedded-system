import QtQuick
import MonitorNews2

// Curva de um parâmetro do PPG com valor, tendência e subescore à esquerda, na mesma linha.
// Toque expande a curva; as outras encolhem sem sumir.
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
    property real phaseOffset: 0
    property int liveChannel: WaveformItem.None
    property bool isExpanded: false
    property bool isCompact: false

    readonly property bool isOnline: Monitor.vitals.hasFingerContact
    readonly property color parameterColor: isOnline ? Theme.parameterColor(key) : Theme.dim
    readonly property bool hasTrend: isOnline && trendDirection !== 0

    signal toggled()

    color: Theme.waveBackground
    border.width: score === 3 ? 3 : 1
    border.color: score === 3 ? Theme.score3 : isExpanded ? parameterColor : Theme.line
    clip: true

    Rectangle {
        id: valuePanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: row.border.width
        width: 200 - row.border.width
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
                visible: !row.isCompact

                UiText {
                    id: headerName
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.name
                    font.pixelSize: 20
                    font.bold: true
                    color: row.parameterColor
                }

                UiText {
                    anchors.left: headerName.right
                    anchors.leftMargin: 6
                    anchors.baseline: headerName.baseline
                    text: row.unit
                    color: Theme.dim
                }

                ProvenanceMark {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    markColor: row.parameterColor
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
                    text: row.isOnline ? row.valueText : "—"
                    font.pixelSize: row.isExpanded ? 68 : row.isCompact ? 40 : 54
                    font.weight: Font.DemiBold
                    font.letterSpacing: -1.5
                    lineHeightMode: Text.FixedHeight
                    lineHeight: font.pixelSize
                    color: row.parameterColor
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
                        text: row.hasTrend ? (row.trendDirection > 0 ? "▲" : "▼") : ""
                        font.pixelSize: 20
                        lineHeightMode: Text.FixedHeight
                        lineHeight: 20
                        opacity: Monitor.blinkPhase ? 1 : 0.25
                    }

                    UiText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: row.hasTrend ? row.trendSince : ""
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
                    width: row.isCompact ? 34 : row.isExpanded ? 42 : 38
                    height: width
                    fontSize: row.isCompact ? 20 : row.isExpanded ? 24 : 22
                    score: row.isOnline ? row.score : -1
                }
            }
        }
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
            phaseOffset: row.phaseOffset
            liveChannel: row.liveChannel
        }

        UiText {
            x: 10
            y: 5
            text: row.name
            font.bold: true
            color: row.parameterColor
        }

        Rectangle {
            anchors.fill: parent
            visible: !row.isOnline
            color: Theme.waveBackground

            HatchItem {
                anchors.fill: parent
                lineColor: Theme.offlineHatch
            }

            Row {
                anchors.centerIn: parent
                spacing: 14

                UiText {
                    id: noSignal
                    text: Monitor.vitals.isWaitingForFinger ? "AGUARDANDO DEDO" : "SEM SINAL"
                    font.pixelSize: 22
                    font.bold: true
                }

                UiText {
                    anchors.baseline: noSignal.baseline
                    text: Monitor.vitals.isWaitingForFinger ? "posicione o dedo no sensor"
                                                            : row.lastReadingText
                    font.pixelSize: 17
                    color: Theme.soft
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: row.toggled()
    }
}
