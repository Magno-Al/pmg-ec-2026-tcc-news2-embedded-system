import QtQuick
import MonitorNews2

// Marca de procedência: quadrado cheio que pisca a cada leitura (medido) ou vazado (manual).
Row {
    id: mark

    property bool isMeasured: true
    property color markColor: Theme.text

    spacing: 6

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 10
        height: 10
        color: mark.isMeasured ? mark.markColor : "transparent"
        opacity: mark.isMeasured && !Monitor.blinkPhase ? 0.35 : 1
        border.width: mark.isMeasured ? 0 : 2
        border.color: Theme.text
    }

    UiText {
        text: mark.isMeasured ? "MEDIDO" : "MANUAL"
        isMono: true
        color: Theme.dim
    }
}
