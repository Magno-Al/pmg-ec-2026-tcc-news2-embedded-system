import QtQuick

// Cabeçalho das telas secundárias: voltar, título e área de ações à direita.
Item {
    id: header

    property string title
    property string subtitle
    property bool isSubtitleMono: false
    default property alias actions: actionRow.data

    signal backRequested()

    height: 64

    TouchButton {
        id: backButton
        anchors.verticalCenter: parent.verticalCenter
        width: 120
        title: "‹ Voltar"
        onClicked: header.backRequested()
    }

    Column {
        anchors.left: backButton.right
        anchors.leftMargin: 14
        anchors.right: actionRow.left
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        UiText {
            width: parent.width
            text: header.title
            font.pixelSize: 24
            font.bold: true
            elide: Text.ElideRight
        }

        UiText {
            width: parent.width
            text: header.subtitle
            isMono: header.isSubtitleMono
            color: Theme.dim
            elide: Text.ElideRight
        }
    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
    }
}
