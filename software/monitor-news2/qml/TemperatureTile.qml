import QtQuick
import MonitorNews2

// Temperatura: sensor 1-Wire separado, continua mesmo sem contato no dedo.
Box {
    id: tile

    readonly property int score: Monitor.vitals.temperatureScore

    fillColor: Theme.panel
    borderWidth: score === 3 ? 3 : 1
    borderColor: score === 3 ? Theme.score3 : Theme.line

    Item {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 7
        anchors.bottomMargin: 7

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 6

            UiText {
                text: "Temp"
                font.pixelSize: 20
                font.bold: true
                color: Theme.temperature
            }

            UiText {
                anchors.baseline: parent.children[0].baseline
                text: "°C"
                color: Theme.dim
            }
        }

        ProvenanceMark {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 4
            markColor: Theme.temperature
        }

        UiText {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: Monitor.vitals.temperatureText
            font.pixelSize: 64
            font.weight: Font.DemiBold
            font.letterSpacing: -1
            color: Theme.temperature
        }

        ScoreBadge {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            score: tile.score
        }

        UiText {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            text: "sensor axilar · ao vivo"
            color: Theme.dim
        }
    }
}
