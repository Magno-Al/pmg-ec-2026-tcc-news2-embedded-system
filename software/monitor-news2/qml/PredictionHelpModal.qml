import QtQuick
import MonitorNews2

// Como funciona a predição: o que responde, quando roda, como agir e onde erra.
Rectangle {
    id: modal

    signal closeRequested()

    color: Theme.scrim

    MouseArea {
        anchors.fill: parent
    }

    Box {
        anchors.centerIn: parent
        width: 940
        height: dialogColumn.implicitHeight + 36
        fillColor: Theme.panel
        borderColor: Theme.modelBorder
        borderStyle: "dashed"
        borderWidth: 2

        Column {
            id: dialogColumn
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            anchors.topMargin: 18
            spacing: 14

            Item {
                width: parent.width
                height: 34

                UiText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Como funciona a predição 6 h"
                    font.pixelSize: 26
                    font.bold: true
                    color: Theme.model
                }

                UiText {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: Monitor.prediction.timing
                    isMono: true
                    color: Theme.dim
                }
            }

            Row {
                width: parent.width
                spacing: 22

                Column {
                    width: parent.width - 380 - parent.spacing
                    spacing: 10

                    Repeater {
                        model: [
                            { title: "O que ela responde",
                              detail: "Uma pergunta só: o NEWS2 deste paciente deve chegar a 5 "
                                      + "ou mais nas próximas 6 horas? O resultado é \"risco de "
                                      + "piora\" ou \"sem alerta\"." },
                            { title: "Quando roda",
                              detail: "De hora em hora, usando as últimas 6 h de sinais vitais. "
                                      + "Até completar 6 h de monitoramento aparece "
                                      + "\"estimando\", com o tempo que falta." },
                            { title: "Como agir",
                              detail: "\"Risco de piora\" antecipa, não confirma: reavalie o "
                                      + "paciente e antecipe aferições. \"Sem alerta\" não "
                                      + "dispensa o protocolo do NEWS2." },
                            { title: "Limites",
                              detail: "Modelo XGBoost com revocação de 0,765: cerca de 1 em cada "
                                      + "4 pioras não é antecipada. Sem o sensor de dedo, a "
                                      + "predição pausa." }
                        ]

                        Column {
                            required property var modelData
                            width: parent.width
                            spacing: 2

                            UiText {
                                text: modelData.title
                                font.pixelSize: 19
                                font.bold: true
                                color: Theme.model
                            }

                            UiText {
                                width: parent.width
                                text: modelData.detail
                                font.pixelSize: 17
                                lineHeight: 1.2
                                wrapMode: Text.WordWrap
                                color: Theme.soft
                            }
                        }
                    }
                }

                Column {
                    width: 380
                    spacing: 8

                    UiText {
                        text: "NEWS2 · últimas 6 h"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    ScoreHistoryChart {
                        width: parent.width
                        height: 150
                        values: {
                            Monitor.revision
                            return Monitor.recentScoreBars(12)
                        }
                        maximumBarHeight: 130
                        isUrgentLabelOnRight: true
                        isEmergencyLineVisible: false
                        areValuesVisible: false
                    }

                    Item {
                        width: parent.width
                        height: 20

                        UiText {
                            anchors.left: parent.left
                            text: "6 h atrás"
                            isMono: true
                            color: Theme.dim
                        }

                        UiText {
                            anchors.right: parent.right
                            text: "agora"
                            isMono: true
                            color: Theme.dim
                        }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 16

                UiText {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - closeButton.width - parent.spacing
                    text: "Desempenho medido: AUROC 0,866 · AUPRC 0,773 · revocação 0,765. "
                          + "Cerca de 1 em cada 4 deteriorações não é antecipada — a predição "
                          + "complementa o NEWS2, não o substitui."
                    lineHeight: 1.2
                    wrapMode: Text.WordWrap
                    color: Theme.soft
                }

                TouchButton {
                    id: closeButton
                    width: 140
                    height: 64
                    title: "Fechar"
                    borderColor: Theme.strongLine
                    onClicked: modal.closeRequested()
                }
            }
        }
    }
}
