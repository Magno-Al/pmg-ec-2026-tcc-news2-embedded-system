import QtQuick
import MonitorNews2

// Evolução desde a admissão: um parâmetro por linha, faixa sombreada = 0 ponto no NEWS2.
Item {
    id: screen

    property string range: "all"
    readonly property var trends: {
        Monitor.revision
        return visible ? Monitor.trends(range) : ({ rows: [], ticks: [], rangeText: "" })
    }

    signal backRequested()

    Column {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 10
        spacing: 6

        ScreenHeader {
            width: parent.width
            title: "Evolução do paciente"
            subtitle: screen.trends.rangeText + " · faixa sombreada = 0 ponto no NEWS2"
            onBackRequested: screen.backRequested()

            Repeater {
                model: [
                    { id: "all", label: "Plantão" },
                    { id: "24", label: "24 h" },
                    { id: "12", label: "12 h" }
                ]

                TouchButton {
                    required property var modelData
                    readonly property bool isSelected: screen.range === modelData.id
                    width: Math.max(96, implicitTitleWidth + 28)
                    readonly property real implicitTitleWidth: modelData.label.length * 10
                    height: 60
                    title: modelData.label
                    titleSize: 19
                    fillColor: isSelected ? Theme.text : Theme.button
                    pressedColor: isSelected ? Theme.soft : Theme.pressed
                    textColor: isSelected ? Theme.ink : Theme.text
                    borderColor: Theme.strongLine
                    onClicked: screen.range = modelData.id
                }
            }
        }

        Column {
            id: rows
            width: parent.width
            height: parent.height - 64 - 20 - 2 * parent.spacing
            spacing: 6

            Repeater {
                model: screen.trends.rows

                Box {
                    required property var modelData
                    readonly property color parameterColor: Theme.parameterColor(modelData.key)

                    width: rows.width
                    height: (rows.height - 4 * rows.spacing) / 5
                    fillColor: Theme.panel
                    borderColor: Theme.divider

                    Item {
                        id: legend
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 178

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 1

                            Item {
                                width: parent.width
                                height: 22

                                UiText {
                                    id: rowName
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.name
                                    font.pixelSize: 19
                                    font.bold: true
                                    color: parameterColor
                                }

                                UiText {
                                    anchors.left: rowName.right
                                    anchors.leftMargin: 6
                                    anchors.baseline: rowName.baseline
                                    text: modelData.unit
                                    color: Theme.dim
                                }

                                UiText {
                                    anchors.right: parent.right
                                    anchors.baseline: rowName.baseline
                                    text: modelData.tag
                                    isMono: true
                                    color: Theme.dim
                                }
                            }

                            UiText {
                                text: modelData.nowText
                                font.pixelSize: 32
                                lineHeightMode: Text.FixedHeight
                                lineHeight: 34
                                font.weight: Font.DemiBold
                                color: parameterColor
                            }

                            UiText {
                                width: parent.width
                                text: modelData.rangeText
                                lineHeightMode: Text.FixedHeight
                                lineHeight: 18
                                color: Theme.soft
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.divider
                        }
                    }

                    TrendPlotItem {
                        anchors.left: legend.right
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        points: modelData.points
                        color: parameterColor
                        bandColor: Theme.normalBand
                        dotFillColor: Theme.panel
                        bandTop: modelData.bandTop
                        bandBottom: modelData.bandBottom
                        isDashed: modelData.isDashed
                        hasDots: modelData.hasDots

                        UiText {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: 6
                            anchors.topMargin: 2
                            text: modelData.axisTop
                            isMono: true
                            color: Theme.hint
                        }

                        UiText {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: 6
                            anchors.bottomMargin: 2
                            text: modelData.axisBottom
                            isMono: true
                            color: Theme.hint
                        }
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: 20

            Repeater {
                id: tickRepeater
                model: screen.trends.ticks

                UiText {
                    required property string modelData
                    required property int index
                    readonly property real plotWidth: parent.width - 178
                    x: 178 + plotWidth * index / (tickRepeater.count - 1)
                       - (index === 0 ? 0 : index === tickRepeater.count - 1 ? width : width / 2)
                    text: modelData
                    isMono: true
                    color: Theme.dim
                }
            }
        }
    }
}
