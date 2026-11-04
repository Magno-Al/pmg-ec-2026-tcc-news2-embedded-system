import QtQuick
import MonitorNews2

// Rodapé alinhado à grade: Inserir PA, Plantão e Evolução (204 px cada) sob as curvas;
// Silenciar e Modo noite sob o painel NEWS2 (372 px). No alarme de NEWS2, a faixa de alerta
// ocupa de Evolução até a borda direita: som e modo noite não podem ser acionados, mas
// Inserir PA e Plantão continuam. Perda de sensor não troca o rodapé — o aviso fica no topo.
Item {
    id: bar

    readonly property var alarm: Monitor.alarm
    readonly property bool isPressureOverdue: Monitor.pressure.isOverdue

    signal registerRequested()
    signal trendsRequested()
    signal handoffRequested()

    height: 76

    Row {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 8
        spacing: 8

        TouchButton {
            width: 204
            height: 60
            title: "Inserir PA"
            subtitle: Monitor.pressure.dueShort
            horizontalAlignment: Text.AlignLeft
            fillColor: bar.isPressureOverdue ? Theme.score2 : Theme.button
            textColor: bar.isPressureOverdue ? Theme.ink : Theme.text
            borderWidth: bar.isPressureOverdue ? 0 : 1
            pressedColor: bar.isPressureOverdue ? Theme.score1 : Theme.pressed
            onClicked: bar.registerRequested()
        }

        TouchButton {
            width: 204
            height: 60
            title: "Plantão"
            subtitle: Monitor.patient.isShiftActive ? Monitor.patient.shiftSince : "iniciar"
            subtitleColor: Theme.dim
            onClicked: bar.handoffRequested()
        }

        Row {
            visible: !bar.alarm.isVisible
            width: parent.width - 2 * 204 - 2 * parent.spacing
            height: 60
            spacing: 8

            TouchButton {
                width: parent.width - 372 - parent.spacing
                height: 60
                title: "Evolução"
                subtitle: Monitor.patient.shiftSince
                isEnabled: Monitor.patient.isShiftActive
                textColor: isEnabled ? Theme.text : Theme.faint
                subtitleColor: isEnabled ? Theme.dim : Theme.faint
                fillColor: isEnabled ? Theme.button : Theme.background
                borderColor: isEnabled ? Theme.buttonLine : Theme.line
                onClicked: bar.trendsRequested()
            }

            MuteButton {
                width: (372 - parent.spacing) / 2
                height: 60
            }

            TouchButton {
                readonly property bool isNight: Monitor.display.isNightMode
                width: (372 - parent.spacing) / 2
                height: 60
                title: isNight ? "Modo dia" : "Modo noite"
                subtitle: isNight ? "brilho normal" : "tela escurece"
                fillColor: isNight ? Theme.text : Theme.button
                textColor: isNight ? Theme.ink : Theme.text
                pressedColor: isNight ? Theme.soft : Theme.pressed
                onClicked: Monitor.toggleNightMode()
            }
        }

        Rectangle {
            visible: bar.alarm.isVisible
            width: parent.width - 2 * 204 - 2 * parent.spacing
            height: 60
            color: Theme.tone(bar.alarm.tone)

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter

                UiText {
                    width: parent.width
                    text: bar.alarm.title
                    font.pixelSize: 21
                    font.bold: true
                    color: Theme.ink
                    elide: Text.ElideRight
                }

                UiText {
                    width: parent.width
                    text: bar.alarm.subtitle
                    font.weight: Font.Medium
                    color: Theme.ink
                    elide: Text.ElideRight
                }
            }
        }
    }
}
