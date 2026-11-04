import QtQuick

// Alto-falante; com isCrossed, cortado (modo mudo).
Canvas {
    id: icon

    property color color: Theme.text
    property bool isCrossed: false

    width: 28
    height: 28
    onColorChanged: requestPaint()
    onIsCrossedChanged: requestPaint()

    onPaint: {
        const context = getContext("2d")
        const scale = width / 24
        context.reset()
        context.scale(scale, scale)
        context.strokeStyle = icon.color
        context.fillStyle = icon.color
        context.lineWidth = 2
        context.lineCap = "round"
        context.lineJoin = "round"

        context.beginPath()
        context.moveTo(3, 9)
        context.lineTo(7, 9)
        context.lineTo(12, 5)
        context.lineTo(12, 19)
        context.lineTo(7, 15)
        context.lineTo(3, 15)
        context.closePath()
        context.fill()
        context.stroke()

        context.beginPath()
        context.arc(12, 12, 4, -Math.PI / 2.6, Math.PI / 2.6)
        context.stroke()
        context.beginPath()
        context.arc(11, 12, 8, -Math.PI / 2.8, Math.PI / 2.8)
        context.stroke()

        if (icon.isCrossed) {
            context.lineWidth = 2.6
            context.beginPath()
            context.moveTo(2, 2)
            context.lineTo(22, 22)
            context.stroke()
        }
    }
}
