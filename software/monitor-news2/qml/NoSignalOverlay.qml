import QtQuick
import MonitorNews2

// Cobre a curva sem o dedo no sensor: "AGUARDANDO DEDO" antes do primeiro contato, "SEM SINAL"
// com a hora da última leitura depois.
Rectangle {
    id: overlay

    property string lastReadingText

    visible: !Monitor.vitals.hasFingerContact
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
                                                    : overlay.lastReadingText
            font.pixelSize: 17
            color: Theme.soft
        }
    }
}
