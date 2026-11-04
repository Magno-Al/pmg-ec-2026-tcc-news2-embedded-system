import QtQuick

// Ação destrutiva: só dispara se mantida pressionada por holdDuration. A barra preenche
// enquanto o dedo está no botão; soltar antes cancela.
Rectangle {
    id: button

    property string title
    property string hint
    property color accentColor: Theme.score3
    property int holdDuration: 2000
    property real progress: 0

    signal held()

    implicitHeight: Theme.minimumTouch
    color: Theme.button
    border.width: 2
    border.color: accentColor
    clip: true

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * button.progress
        color: button.accentColor
    }

    Column {
        anchors.centerIn: parent
        spacing: 1

        UiText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: button.title
            font.pixelSize: 20
            font.bold: true
            color: button.progress > 0.5 ? Theme.ink : Theme.text
        }

        UiText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: button.hint
            color: button.progress > 0.5 ? Theme.ink : Theme.dim
        }
    }

    NumberAnimation {
        id: holdAnimation
        target: button
        property: "progress"
        from: 0
        to: 1
        duration: button.holdDuration
        onFinished: {
            if (button.progress >= 1)
                button.held()
            button.progress = 0
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: holdAnimation.start()
        onReleased: {
            holdAnimation.stop()
            button.progress = 0
        }
        onCanceled: {
            holdAnimation.stop()
            button.progress = 0
        }
    }
}
