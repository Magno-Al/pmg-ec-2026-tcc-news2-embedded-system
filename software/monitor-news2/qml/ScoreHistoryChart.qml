import QtQuick

// Barras do NEWS2 por janela de 30 min, com as linhas de limiar 5 (urgente) e 7 (emergência).
// values: lista de inteiros; -1 = sem dado na janela.
Item {
    id: chart

    property var values: []
    property real maximumBarHeight: 96
    property bool areValuesVisible: true
    property var visibleValueIndexes: []
    property bool isEmergencyLineVisible: true
    property bool isUrgentLabelOnRight: false

    function barHeight(value) {
        return value < 0 ? 0 : Math.max(4, Math.round(value / 8 * maximumBarHeight))
    }

    function barColor(value) {
        if (value >= 7) return Theme.score3
        if (value >= 5) return Theme.score2
        if (value >= 1) return Theme.score1
        return Theme.score0
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.strongLine
    }

    Row {
        anchors.fill: parent
        spacing: 4

        Repeater {
            model: chart.values

            Item {
                required property var modelData
                required property int index

                width: (chart.width - (chart.values.length - 1) * 4) / chart.values.length
                height: chart.height

                Rectangle {
                    id: bar
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: chart.barHeight(modelData)
                    color: modelData < 0 ? "transparent" : chart.barColor(modelData)
                }

                UiText {
                    anchors.bottom: bar.top
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: chart.areValuesVisible && modelData >= 0
                             && chart.visibleValueIndexes.indexOf(index) >= 0
                    text: String(modelData)
                    font.weight: Font.DemiBold
                    color: Theme.soft
                }
            }
        }
    }

    ThresholdLine {
        level: 5
        lineColor: Theme.score2
        label: chart.isUrgentLabelOnRight ? "limiar 5" : "5 urgente"
        isLabelOnRight: chart.isUrgentLabelOnRight
    }

    ThresholdLine {
        visible: chart.isEmergencyLineVisible
        level: 7
        lineColor: Theme.score3
        label: "7 emergência"
    }

    component ThresholdLine: Item {
        id: threshold

        property int level
        property color lineColor
        property string label
        property bool isLabelOnRight: false

        anchors.left: parent.left
        anchors.right: parent.right
        y: chart.height - Math.round(level / 8 * chart.maximumBarHeight)
        height: 2

        Row {
            anchors.fill: parent
            spacing: 4

            Repeater {
                model: Math.max(0, Math.ceil(threshold.width / 9))

                Rectangle {
                    width: 5
                    height: 2
                    color: threshold.lineColor
                }
            }
        }

        UiText {
            anchors.bottom: parent.top
            anchors.bottomMargin: 2
            anchors.left: threshold.isLabelOnRight ? undefined : parent.left
            anchors.right: threshold.isLabelOnRight ? parent.right : undefined
            text: threshold.label
            color: threshold.lineColor
        }
    }
}
