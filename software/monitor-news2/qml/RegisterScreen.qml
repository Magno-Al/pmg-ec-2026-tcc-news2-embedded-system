import QtQuick
import MonitorNews2

// Registro manual de PA sistólica e ar/O₂: digitar, conferir e só então gravar.
// O oxigênio não vem pré-selecionado: a enfermagem confirma a condição atual a cada registro.
Item {
    id: screen

    readonly property var pressure: Monitor.pressure
    property bool isConfirming: false

    signal backRequested()

    function reset() {
        entry.reset()
        isConfirming = false
    }

    PressureEntry {
        id: entry
    }

    readonly property int projectedTotal: entry.isInRange
                                          ? Monitor.projectedTotal(entry.value, entry.oxygenChoice)
                                          : -1
    readonly property int pressureDifference: pressure.hasRecord && entry.isInRange
                                              ? Math.abs(entry.value - pressure.previousValue)
                                              : 0

    Column {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 12
        spacing: 8

        ScreenHeader {
            width: parent.width
            title: "Registro manual · PA sistólica e oxigênio"
            subtitle: "Leito " + Monitor.patient.bed
            onBackRequested: screen.backRequested()

            StepTag { text: "1 DIGITAR"; isCurrent: !screen.isConfirming }
            StepTag { text: "2 CONFERIR"; isCurrent: screen.isConfirming }
        }

        Row {
            width: parent.width
            height: parent.height - 64 - parent.spacing
            spacing: 12

            Column {
                width: parent.width - 440 - parent.spacing
                height: parent.height
                spacing: 10

                Box {
                    width: parent.width
                    height: parent.height - oxygenBox.height - parent.spacing
                    fillColor: Theme.panel

                    Item {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 12
                        anchors.bottomMargin: 12

                        UiText {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            text: "PA SISTÓLICA · mmHg"
                            font.pixelSize: 18
                            font.bold: true
                            font.letterSpacing: 0.7
                        }

                        UiText {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            text: screen.pressure.previousText
                            font.pixelSize: 17
                            color: Theme.dim
                        }

                        UiText {
                            id: digitsText
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: -6
                            text: entry.digits.length > 0 ? entry.digits : "– – –"
                            font.pixelSize: 112
                            font.weight: Font.DemiBold
                            color: entry.digits.length > 0 ? Theme.text : Theme.faint
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: message.top
                            anchors.bottomMargin: 8
                            height: 3
                            color: entry.isInRange ? Theme.text
                                   : entry.digits.length >= 2 ? Theme.score2 : Theme.faint
                        }

                        UiText {
                            id: message
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            font.pixelSize: 19
                            text: {
                                if (entry.digits.length === 0)
                                    return "Digite a PA sistólica"
                                if (!entry.isInRange)
                                    return entry.digits.length < 2
                                           ? "Continue digitando"
                                           : "Fora da faixa aceita (50–300 mmHg)"
                                const points = Monitor.pressureScore(entry.value)
                                return "PA " + points + (points === 1 ? " pt" : " pts")
                                       + " · NEWS2 ficaria " + screen.projectedTotal
                                       + (entry.oxygenChoice < 0 ? " (falta O₂)" : "")
                            }
                            color: entry.digits.length >= 2 && !entry.isInRange ? Theme.score2
                                   : entry.isInRange ? Theme.text : Theme.dim
                        }
                    }
                }

                Box {
                    id: oxygenBox
                    width: parent.width
                    height: 118
                    fillColor: Theme.panel

                    Column {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 12
                        spacing: 8

                        Row {
                            spacing: 6

                            UiText {
                                text: "OXIGÊNIO AGORA"
                                font.pixelSize: 18
                                font.bold: true
                                font.letterSpacing: 0.7
                            }

                            UiText {
                                text: "· sem pré-seleção: confirme a condição atual"
                                font.pixelSize: 18
                                color: Theme.dim
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8

                            OxygenOption {
                                width: (parent.width - parent.spacing) / 2
                                label: "Ar ambiente"
                                points: "0 pt"
                                isSelected: entry.oxygenChoice === 0
                                onClicked: entry.oxygenChoice = 0
                            }

                            OxygenOption {
                                width: (parent.width - parent.spacing) / 2
                                label: "O₂ suplementar"
                                points: "2 pts"
                                isSelected: entry.oxygenChoice === 1
                                onClicked: entry.oxygenChoice = 1
                            }
                        }
                    }
                }
            }

            Column {
                width: 440
                height: parent.height
                spacing: 8

                Grid {
                    id: keypad
                    width: parent.width
                    height: parent.height - confirmButton.height - parent.spacing
                    columns: 3
                    spacing: 8

                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "Limpar", "0", "⌫"]

                        TouchButton {
                            required property string modelData
                            width: (keypad.width - 2 * keypad.spacing) / 3
                            height: (keypad.height - 3 * keypad.spacing) / 4
                            title: modelData
                            titleSize: modelData.length > 1 ? 22 : 40
                            pressedColor: Theme.keyPressed
                            onClicked: entry.pressKey(modelData)
                        }
                    }
                }

                TouchButton {
                    id: confirmButton
                    width: parent.width
                    height: 72
                    title: "Conferir ›"
                    titleSize: 24
                    isEnabled: entry.canConfirm
                    fillColor: entry.canConfirm ? Theme.text : Theme.button
                    pressedColor: Theme.soft
                    textColor: entry.canConfirm ? Theme.ink : Theme.faint
                    borderWidth: 0
                    onClicked: screen.isConfirming = true
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: screen.isConfirming
        color: Theme.strongScrim

        MouseArea {
            anchors.fill: parent
        }

        Box {
            anchors.centerIn: parent
            width: 680
            height: confirmColumn.implicitHeight + 40
            fillColor: Theme.panel
            borderColor: Theme.strongLine

            Column {
                id: confirmColumn
                anchors.fill: parent
                anchors.margins: 20
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 14

                UiText {
                    text: "Confira antes de gravar"
                    font.pixelSize: 26
                    font.bold: true
                }

                Grid {
                    columns: 2
                    columnSpacing: 0
                    rowSpacing: 10
                    verticalItemAlignment: Grid.AlignVCenter

                    ReviewLabel { text: "PA sistólica" }
                    Column {
                        Row {
                            spacing: 8

                            UiText {
                                text: String(entry.value)
                                font.pixelSize: 64
                                font.bold: true
                            }

                            UiText {
                                anchors.baseline: parent.children[0].baseline
                                text: "mmHg"
                                font.pixelSize: 24
                                font.weight: Font.Medium
                            }
                        }

                        UiText {
                            text: "(" + entry.valueInWords + ")"
                            font.pixelSize: 19
                            font.italic: true
                            color: Theme.soft
                        }
                    }

                    ReviewLabel { text: "Oxigênio" }
                    UiText {
                        text: entry.oxygenChoice === 1 ? "O₂ suplementar" : "Ar ambiente"
                        font.pixelSize: 26
                        font.bold: true
                    }

                    ReviewLabel { text: "NEWS2" }
                    UiText {
                        text: (Monitor.score.isComplete ? Monitor.score.total : "—")
                              + " → " + screen.projectedTotal
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                }

                Rectangle {
                    width: parent.width
                    height: warningText.implicitHeight + 20
                    visible: screen.pressureDifference >= 30
                    color: Theme.score2

                    UiText {
                        id: warningText
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        text: "Diferença de " + screen.pressureDifference
                              + " mmHg em relação ao registro anterior ("
                              + screen.pressure.previousValue
                              + "). Se houver dúvida, reafira antes de gravar."
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: Theme.ink
                        wrapMode: Text.WordWrap
                    }
                }

                Row {
                    width: parent.width
                    spacing: 10

                    TouchButton {
                        width: (parent.width - parent.spacing) / 2.4
                        height: 72
                        title: "Corrigir"
                        titleSize: 22
                        borderColor: Theme.strongLine
                        onClicked: screen.isConfirming = false
                    }

                    TouchButton {
                        width: (parent.width - parent.spacing) * 1.4 / 2.4
                        height: 72
                        title: "Gravar registro"
                        titleSize: 22
                        fillColor: Theme.text
                        pressedColor: Theme.soft
                        textColor: Theme.ink
                        borderWidth: 0
                        onClicked: {
                            Monitor.recordManualEntry(entry.value, entry.oxygenChoice === 1)
                            screen.backRequested()
                        }
                    }
                }
            }
        }
    }

    component StepTag: Rectangle {
        property alias text: label.text
        property bool isCurrent: false

        width: label.implicitWidth + 20
        height: label.implicitHeight + 12
        color: isCurrent ? Theme.text : Theme.button

        UiText {
            id: label
            anchors.centerIn: parent
            isMono: true
            color: parent.isCurrent ? Theme.ink : Theme.dim
        }
    }

    component ReviewLabel: UiText {
        width: 170
        font.pixelSize: 19
        color: Theme.dim
    }

    component OxygenOption: Rectangle {
        id: option

        property string label
        property string points
        property bool isSelected: false

        signal clicked()

        height: 72
        color: isSelected ? Theme.text : Theme.button
        border.width: isSelected ? 3 : 1
        border.color: isSelected ? Theme.text : Theme.strongLine

        UiText {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: option.label
            font.pixelSize: 22
            font.bold: true
            color: option.isSelected ? Theme.ink : Theme.text
        }

        UiText {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: option.points
            font.pixelSize: 17
            font.weight: Font.Medium
            color: option.isSelected ? Theme.ink : Theme.text
        }

        MouseArea {
            anchors.fill: parent
            onClicked: option.clicked()
        }
    }
}
