import QtQuick

// Subescore do NEWS2 (0–3); -1 significa parâmetro sem dado.
Box {
    id: badge

    property int score: -1
    property int fontSize: 26

    readonly property bool isUnknown: score < 0

    fillColor: score > 0 ? Theme.severity(score) : "transparent"
    borderStyle: isUnknown ? "dashed" : "solid"
    borderWidth: 2
    borderColor: isUnknown ? Theme.faint : score > 0 ? Theme.severity(score) : Theme.line

    UiText {
        anchors.centerIn: parent
        text: badge.isUnknown ? "?" : String(badge.score)
        font.pixelSize: badge.fontSize
        font.bold: true
        color: badge.score > 0 ? Theme.ink : badge.isUnknown ? Theme.faint : Theme.dim
    }
}
