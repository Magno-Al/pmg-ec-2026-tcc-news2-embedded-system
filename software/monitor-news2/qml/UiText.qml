import QtQuick

// Texto padrão: Plex Condensed com numerais tabulares, para os valores alinharem.
Text {
    property bool isMono: false

    color: Theme.text
    font.family: isMono ? Theme.mono : Theme.sans
    font.pixelSize: 16
    font.features: { "tnum": 1 }
    textFormat: Text.PlainText
}
