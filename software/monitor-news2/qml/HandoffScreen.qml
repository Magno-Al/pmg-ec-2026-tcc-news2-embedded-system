import QtQuick
import MonitorNews2

// Plantão: ao iniciar, o monitor passa a registrar o que acontece e o histórico do NEWS2, e a
// Evolução fica disponível. Finalizar (segurando 2 s) apaga todos os registros do paciente —
// log, histórico, evolução, PA/O₂ e a janela da predição — para o próximo começar do zero.
Item {
    id: screen

    readonly property var summary: {
        Monitor.revision
        return visible ? Monitor.handoffSummary()
                       : ({ isActive: false, bars: [], ticks: [], events: [] })
    }
    readonly property bool isActive: summary.isActive === true

    signal backRequested()

    Column {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 12
        spacing: 8

        ScreenHeader {
            width: parent.width
            title: "Plantão"
            subtitle: screen.isActive
                      ? screen.summary.rangeText + " · " + screen.summary.statsText
                      : "nenhum plantão em andamento"
            isSubtitleMono: true
            onBackRequested: screen.backRequested()

            TouchButton {
                visible: !screen.isActive
                width: 220
                height: 60
                title: "Iniciar plantão"
                fillColor: Theme.text
                pressedColor: Theme.soft
                textColor: Theme.ink
                borderWidth: 0
                onClicked: Monitor.startShift()
            }

            HoldButton {
                visible: screen.isActive
                width: 220
                height: 60
                title: "Finalizar plantão"
                hint: "segure 2 s · apaga registros"
                onHeld: Monitor.finishShift()
            }
        }

        Box {
            width: parent.width
            height: 172
            fillColor: Theme.panel

            Column {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 10
                anchors.bottomMargin: 6
                spacing: 4
                visible: screen.isActive

                ScoreHistoryChart {
                    width: parent.width
                    height: parent.height - 24
                    maximumBarHeight: 96
                    values: screen.summary.bars.map(bar => bar.value)
                    visibleValueIndexes: screen.summary.bars
                        .map((bar, index) => bar.isLabelVisible ? index : -1)
                        .filter(index => index >= 0)
                }

                Item {
                    width: parent.width
                    height: 20

                    Repeater {
                        id: handoffTicks
                        model: screen.summary.ticks

                        UiText {
                            required property string modelData
                            required property int index
                            x: (parent.width - width) * index / (handoffTicks.count - 1)
                            text: modelData
                            isMono: true
                            color: Theme.dim
                        }
                    }
                }
            }

            EmptyNotice {
                visible: !screen.isActive
                text: "O histórico do NEWS2 começa quando o plantão é iniciado."
            }
        }

        Box {
            width: parent.width
            height: parent.height - 64 - 172 - 2 * parent.spacing
            fillColor: Theme.panel
            clip: true

            Column {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 10
                spacing: 3

                UiText {
                    text: "O QUE ACONTECEU"
                    font.pixelSize: 18
                    font.bold: true
                    font.letterSpacing: 0.7
                    bottomPadding: 2
                }

                Repeater {
                    model: screen.summary.events

                    Row {
                        required property var modelData
                        width: parent.width
                        spacing: 12

                        UiText {
                            text: modelData.time
                            isMono: true
                            font.pixelSize: 17
                            color: Theme.dim
                        }

                        Box {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 10
                            fillColor: Theme.eventColor(modelData.kind)
                            borderWidth: modelData.kind === "registration"
                                         || modelData.kind === "prediction" ? 2 : 0
                            borderStyle: modelData.kind === "prediction" ? "dashed" : "solid"
                            borderColor: modelData.kind === "prediction" ? Theme.model
                                                                         : Theme.text
                        }

                        UiText {
                            width: parent.width - 100
                            text: modelData.text
                            font.pixelSize: 17
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            EmptyNotice {
                visible: !screen.isActive
                text: "Inicie o plantão para registrar eventos, alarmes e registros de PA."
            }
        }
    }

    component EmptyNotice: UiText {
        anchors.centerIn: parent
        font.pixelSize: 18
        color: Theme.dim
    }
}
