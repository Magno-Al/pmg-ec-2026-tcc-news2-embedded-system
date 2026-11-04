import QtQuick
import MonitorNews2

// Identificação sempre visível, inclusive nos fluxos de registro.
Rectangle {
    height: 52
    color: Theme.topBar

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.topBarLine
    }

    Rectangle {
        id: bedBadge
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: 36
        width: bedText.implicitWidth + 20
        color: Theme.text

        UiText {
            id: bedText
            anchors.centerIn: parent
            text: Monitor.patient.bed
            font.pixelSize: 22
            font.bold: true
            color: Theme.ink
        }
    }

    Row {
        anchors.right: clock.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        SensorChip {
            readonly property bool hasContact: Monitor.vitals.hasFingerContact
            readonly property bool isBright: Monitor.blinkPhase
                                             || !Monitor.alarm.isTechnicalSoundOn
            label: hasContact ? "PPG" : "PPG SEM CONTATO"
            dotColor: hasContact ? Theme.sensorOk : Theme.ink
            fillColor: hasContact ? "transparent" : isBright ? Theme.score2 : Theme.button
            textColor: hasContact ? Theme.dim : Theme.ink
            borderColor: hasContact ? Theme.line : Theme.score2
        }

        SensorChip {
            label: "TEMP"
            dotColor: Theme.sensorOk
        }
    }

    UiText {
        id: clock
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: Monitor.clockText
        font.pixelSize: 24
        font.weight: Font.DemiBold
    }

    component SensorChip: Rectangle {
        id: chip

        property string label
        property color dotColor
        property color fillColor: "transparent"
        property color textColor: Theme.dim
        property color borderColor: Theme.line

        height: 32
        width: chipRow.implicitWidth + 18
        color: fillColor
        border.width: 1
        border.color: borderColor

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 7

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                color: chip.dotColor
            }

            UiText {
                text: chip.label
                isMono: true
                font.weight: Font.Medium
                color: chip.textColor
            }
        }
    }
}
