import QtQuick
import MonitorNews2

// Painel retangular com borda sólida, tracejada ou dupla.
Item {
    id: box

    property color fillColor: "transparent"
    property color borderColor: Theme.line
    property real borderWidth: 1
    property string borderStyle: "solid"

    Rectangle {
        anchors.fill: parent
        color: box.fillColor
        border.width: box.borderStyle === "solid" ? box.borderWidth : 0
        border.color: box.borderColor
    }

    FrameItem {
        anchors.fill: parent
        visible: box.borderStyle !== "solid" && box.borderWidth > 0
        color: box.borderColor
        lineWidth: box.borderWidth
        style: box.borderStyle === "double" ? FrameItem.Double : FrameItem.Dashed
    }
}
