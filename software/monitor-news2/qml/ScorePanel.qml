import QtQuick
import MonitorNews2

// NEWS2 e predição num só painel: a parte sólida é o escore calculado; a caixa hachurada e
// tracejada, na cor exclusiva do modelo, é a predição — estimativa nunca parece medida.
Box {
    id: panel

    readonly property var score: Monitor.score
    readonly property var prediction: Monitor.prediction
    readonly property color bandColor: score.bandIndex > 0 ? Theme.severity(score.bandIndex)
                                                           : Theme.text
    readonly property bool isScoreFlashOff: score.isSoundOn && !Monitor.blinkPhase

    signal helpRequested()

    fillColor: Theme.panel
    borderColor: Theme.line

    Column {
        id: scoreSection
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        topPadding: 10
        bottomPadding: 12
        leftPadding: 12
        rightPadding: 12
        spacing: 8

        Item {
            width: parent.width - 24
            height: 28

            UiText {
                anchors.left: parent.left
                anchors.baseline: parent.bottom
                anchors.baselineOffset: -6
                text: "NEWS2"
                font.pixelSize: 22
                font.bold: true
                font.letterSpacing: 1
            }

            UiText {
                anchors.right: parent.right
                anchors.baseline: parent.bottom
                anchors.baselineOffset: -6
                text: panel.score.calculationText
                isMono: true
                color: Theme.dim
            }
        }

        Row {
            width: parent.width - 24
            height: 96
            spacing: 8

            Box {
                id: scoreBox
                readonly property bool isSolidFill: panel.score.isFilled
                                                    && panel.score.bandIndex > 0
                                                    && !panel.isScoreFlashOff
                width: 104
                height: parent.height
                fillColor: isSolidFill ? panel.bandColor : "transparent"
                borderStyle: panel.score.isFilled ? "solid" : "dashed"
                borderWidth: 3
                borderColor: !panel.score.isFilled ? Theme.faint
                             : panel.score.bandIndex > 0 ? panel.bandColor : Theme.line

                UiText {
                    anchors.centerIn: parent
                    text: panel.score.text
                    font.pixelSize: text.length > 1 ? 64 : 80
                    font.bold: true
                    color: scoreBox.isSolidFill ? Theme.ink
                           : panel.score.bandIndex > 0 && panel.score.isFilled
                             ? panel.bandColor : Theme.text
                }
            }

            // Região de detalhes: faixa de resposta e conduta do protocolo.
            Box {
                width: parent.width - scoreBox.width - parent.spacing
                height: parent.height
                borderStyle: "dashed"
                borderColor: Theme.detailLine
                borderWidth: 1

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 5

                    UiText {
                        width: parent.width
                        text: panel.score.title
                        font.pixelSize: 21
                        font.bold: true
                        color: Theme.tone(panel.score.titleTone)
                        wrapMode: Text.WordWrap
                    }

                    UiText {
                        width: parent.width
                        text: panel.score.action
                        font.pixelSize: 17
                        wrapMode: Text.WordWrap
                        lineHeight: 1.05
                    }
                }
            }
        }

        Row {
            width: parent.width - 24
            spacing: 4

            Repeater {
                model: Monitor.ribbon

                Column {
                    required property var modelData
                    readonly property int cellScore: modelData.score

                    width: (scoreSection.width - 24 - 5 * 4) / 6
                    spacing: 3

                    UiText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: Theme.dim
                    }

                    Box {
                        width: parent.width
                        height: 32
                        fillColor: cellScore > 0 ? Theme.severity(cellScore) : "transparent"
                        borderStyle: cellScore < 0 ? "dashed"
                                     : modelData.provenance === "recorded" ? "double" : "solid"
                        borderWidth: modelData.provenance === "recorded" && cellScore >= 0 ? 4 : 2
                        borderColor: cellScore < 0 ? Theme.faint
                                     : modelData.provenance === "recorded"
                                       ? (cellScore > 0 ? Theme.ink : Theme.dim)
                                       : (cellScore > 0 ? Theme.severity(cellScore) : Theme.line)

                        UiText {
                            anchors.centerIn: parent
                            text: cellScore < 0 ? "?" : String(cellScore)
                            font.pixelSize: 20
                            font.bold: true
                            color: cellScore > 0 ? Theme.ink
                                   : cellScore < 0 ? Theme.faint : Theme.dim
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: predictionSection
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: scoreSection.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 13
        anchors.rightMargin: 13
        anchors.bottomMargin: 13
        color: Theme.modelPanel

        HatchItem {
            anchors.fill: parent
            lineColor: Theme.modelHatch
        }

        FrameItem {
            anchors.fill: parent
            color: Theme.modelBorder
            lineWidth: 2
        }

        Item {
            id: predictionHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 8
            height: 30

            UiText {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: panel.prediction.title
                font.pixelSize: 21
                font.bold: true
                color: Theme.model
            }

            // Botão de 30 px no desenho; a área de toque se estende até 60 × 60.
            Rectangle {
                id: helpButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                color: helpTapArea.pressed ? Theme.modelPressed : Theme.background
                border.width: 1
                border.color: Theme.model

                UiText {
                    anchors.centerIn: parent
                    text: "?"
                    font.pixelSize: 20
                    font.bold: true
                    color: Theme.model
                }

                MouseArea {
                    id: helpTapArea
                    anchors.centerIn: parent
                    width: Theme.minimumTouch
                    height: Theme.minimumTouch
                    onClicked: panel.helpRequested()
                }
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: predictionHeader.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 10
            spacing: Math.max(6, (height - statusBox.height - nextRun.height
                                  - lastResult.height) / 3)

            Box {
                id: statusBox
                readonly property bool isPositive: panel.prediction.state === "risk"
                readonly property bool isDashed: panel.prediction.state === "collecting"
                                                 || panel.prediction.state === "paused"
                                                 || panel.prediction.state === "idle"
                width: parent.width
                height: 52
                fillColor: isPositive ? Theme.model : "transparent"
                borderColor: Theme.model
                borderWidth: 2
                borderStyle: isDashed ? "dashed" : "solid"
                opacity: panel.prediction.state === "collecting"
                         && !panel.prediction.isSlowBlinkOn ? 0.35 : 1

                Behavior on opacity {
                    NumberAnimation { duration: 1400 }
                }

                UiText {
                    anchors.centerIn: parent
                    text: panel.prediction.headline
                    font.pixelSize: 26
                    font.bold: true
                    font.letterSpacing: 0.8
                    color: statusBox.isPositive ? Theme.ink : Theme.model
                }
            }

            Column {
                id: nextRun
                width: parent.width
                spacing: 6

                UiText {
                    text: panel.prediction.nextRunText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    width: parent.width
                    height: 10
                    color: Theme.modelTrack

                    Rectangle {
                        width: parent.width * panel.prediction.progress
                        height: parent.height
                        color: Theme.model
                    }
                }
            }

            UiText {
                id: lastResult
                width: parent.width
                text: panel.prediction.lastResultText
                font.pixelSize: 17
                elide: Text.ElideRight
                color: Theme.modelSoft
            }
        }
    }
}
