import QtQuick
import MonitorNews2

// Modo mudo temporário: pisca na cor do alarme enquanto toca; tocar pausa o som por 2 min
// (1 min em emergência), tocar de novo cancela. Não existe silêncio permanente.
Rectangle {
    id: muteButton

    readonly property bool isMuted: Monitor.alarm.isMuted
    readonly property bool isSounding: Monitor.alarm.isSounding
    readonly property color soundColor: Theme.tone(Monitor.alarm.soundTone)
    readonly property bool isFlashOn: isSounding && Monitor.blinkPhase
    readonly property color inkColor: isMuted || isFlashOn ? Theme.ink : Theme.text

    implicitHeight: Theme.minimumTouch
    color: isMuted ? Theme.text : isFlashOn ? soundColor : Theme.button
    border.width: isMuted || isSounding ? 2 : 1
    border.color: isMuted ? Theme.text : isSounding ? soundColor : Theme.buttonLine

    Column {
        anchors.centerIn: parent
        spacing: 1

        SpeakerIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            color: muteButton.inkColor
            isCrossed: muteButton.isMuted
        }

        UiText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Monitor.alarm.muteText
            isMono: true
            font.weight: Font.DemiBold
            color: muteButton.inkColor
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: Monitor.toggleMute()
    }
}
