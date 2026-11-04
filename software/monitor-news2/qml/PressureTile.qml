import QtQuick
import MonitorNews2

// PA e ar/O₂ registrados pela enfermagem: o dado envelhece. A barra mede a idade relativa ao
// intervalo que o protocolo exige para o escore atual; vencida, o valor fica riscado.
// Só visualização: o registro entra pelo botão Inserir PA do rodapé.
Box {
    id: tile

    readonly property var pressure: Monitor.pressure

    fillColor: Theme.panel
    borderStyle: pressure.border === "overdue" ? "dashed" : "solid"
    borderWidth: pressure.border === "critical" ? 3 : pressure.border === "overdue" ? 2 : 1
    borderColor: pressure.border === "normal" ? Theme.line : Theme.score3

    Item {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 7
        anchors.bottomMargin: 7

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 6

            UiText {
                text: "PA sistólica"
                font.pixelSize: 20
                font.bold: true
            }

            UiText {
                anchors.baseline: parent.children[0].baseline
                text: "mmHg"
                color: Theme.dim
            }
        }

        ProvenanceMark {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 4
            isMeasured: false
        }

        Row {
            id: valueRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 30
            height: 64
            spacing: 12

            UiText {
                id: pressureValue
                anchors.verticalCenter: parent.verticalCenter
                text: tile.pressure.valueText
                font.pixelSize: 54
                font.weight: Font.DemiBold
                font.strikeout: tile.pressure.isStruck
                opacity: tile.pressure.opacity
            }

            ScoreBadge {
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 44
                score: tile.pressure.score
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 48
                color: Theme.divider
            }

            Item {
                anchors.verticalCenter: parent.verticalCenter
                width: valueRow.width - pressureValue.width - 44 - 1 - 3 * valueRow.spacing
                height: 48

                UiText {
                    anchors.right: oxygenBadge.left
                    anchors.rightMargin: 8
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: tile.pressure.oxygenText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.WordWrap
                    lineHeight: 0.95
                    opacity: tile.pressure.opacity
                }

                ScoreBadge {
                    id: oxygenBadge
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 38
                    height: 38
                    fontSize: 22
                    score: tile.pressure.oxygenScore
                }
            }
        }

        Rectangle {
            id: ageTrack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: ageText.top
            anchors.bottomMargin: 4
            height: 5
            color: Theme.topBarLine

            Rectangle {
                height: parent.height
                width: parent.width * tile.pressure.ageFraction
                color: Theme.tone(tile.pressure.ageTone)
            }
        }

        UiText {
            id: ageText
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            text: tile.pressure.ageText
            color: Theme.tone(tile.pressure.ageTone)
        }
    }
}
