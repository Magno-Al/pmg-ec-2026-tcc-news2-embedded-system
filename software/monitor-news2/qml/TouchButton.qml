import QtQuick

// Alvo de toque mínimo de 60 × 60 px (dedo enluvado). Sem hover: só estado pressionado.
Rectangle {
    id: button

    property string title
    property string subtitle
    property int titleSize: 20
    property int subtitleSize: 16
    property bool isSubtitleMono: false
    property color textColor: Theme.text
    property color subtitleColor: textColor
    property color fillColor: Theme.button
    property color pressedColor: Theme.pressed
    property color borderColor: Theme.buttonLine
    property int borderWidth: 1
    property int horizontalAlignment: Text.AlignHCenter
    property bool isEnabled: true
    default property alias content: extraContent.data

    signal clicked()

    implicitHeight: Theme.minimumTouch
    implicitWidth: Theme.minimumTouch
    color: tapArea.pressed && isEnabled ? pressedColor : fillColor
    border.width: borderWidth
    border.color: borderColor

    Column {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 1

        Item {
            id: extraContent
            width: parent.width
            height: childrenRect.height
            visible: children.length > 0
        }

        UiText {
            width: parent.width
            visible: button.title.length > 0
            text: button.title
            font.pixelSize: button.titleSize
            font.bold: true
            color: button.textColor
            horizontalAlignment: button.horizontalAlignment
            elide: Text.ElideRight
        }

        UiText {
            width: parent.width
            visible: button.subtitle.length > 0
            text: button.subtitle
            isMono: button.isSubtitleMono
            font.pixelSize: button.subtitleSize
            font.weight: button.isSubtitleMono ? Font.DemiBold : Font.Normal
            color: button.subtitleColor
            horizontalAlignment: button.horizontalAlignment
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: tapArea
        anchors.fill: parent
        enabled: button.isEnabled
        onClicked: button.clicked()
    }
}
