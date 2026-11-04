pragma Singleton

import QtQuick

// Tokens do design "Monitor NEWS2 v2" (oklch convertido para sRGB).
// Cores de identidade dos parâmetros evitam amarelo, laranja e vermelho: cor de sinal nunca é
// lida como gravidade. Gravidade usa a convenção da tabela RCP: 0 contorno, 1 amarelo,
// 2 laranja, 3 vermelho, sempre com tinta escura sobre a cor.
QtObject {
    readonly property color background: "#07080A"
    readonly property color waveBackground: "#0B0D10"
    readonly property color topBar: "#0D1012"
    readonly property color panel: "#101316"
    readonly property color button: "#1E2124"
    readonly property color topBarLine: "#26292D"
    readonly property color divider: "#2A2E33"
    readonly property color line: "#2F3338"
    readonly property color pressed: "#34383D"
    readonly property color buttonLine: "#393E42"
    readonly property color detailLine: "#494E52"
    readonly property color strongLine: "#51565B"
    readonly property color keyPressed: "#51565B"
    readonly property color faint: "#70757A"
    readonly property color hint: "#82878C"
    readonly property color caption: "#A0A5AB"
    readonly property color dim: "#B3B8BE"
    readonly property color soft: "#CCD2D7"
    readonly property color text: "#EFF2F5"
    readonly property color ink: "#0B0E11"

    readonly property color heartRate: "#6FEA77"
    readonly property color oxygenSaturation: "#49E4FC"
    readonly property color respiratoryRate: "#A7B7FF"
    readonly property color temperature: "#F8D8B4"
    readonly property color pressure: "#EFF2F5"

    readonly property color model: "#ED9EE5"
    readonly property color modelPanel: "#120E11"
    readonly property color modelTrack: "#342A33"
    readonly property color modelDim: "#CCC0CA"
    readonly property color modelSoft: "#D9CCD7"
    readonly property color modelBorder: "#99ED9EE5"
    readonly property color modelHatch: "#14ED9EE5"
    readonly property color modelPressed: "#2A2029"

    readonly property color score0: "#44484D"
    readonly property color score1: "#F6D732"
    readonly property color score2: "#FF9127"
    readonly property color score3: "#F93534"
    readonly property color sensorOk: "#6FC082"
    readonly property color selectedOk: "#1F3626"

    readonly property color offlineHatch: "#2E7C8186"
    readonly property color normalBand: "#592D3C30"
    readonly property color scrim: "#E0030304"
    readonly property color strongScrim: "#E6030304"

    readonly property string sans: "IBM Plex Sans Condensed"
    readonly property string mono: "IBM Plex Mono"

    readonly property int minimumTouch: 60

    function severity(index) {
        return [score0, score1, score2, score3][Math.max(0, Math.min(3, index))]
    }

    function tone(name) {
        switch (name) {
        case "dim": return dim
        case "faint": return faint
        case "yellow": case "1": return score1
        case "orange": case "2": return score2
        case "red": case "3": return score3
        case "model": return model
        case "white": return text
        case "0": return text
        default: return text
        }
    }

    function parameterColor(key) {
        switch (key) {
        case "hr": return heartRate
        case "spo2": return oxygenSaturation
        case "rr": return respiratoryRate
        case "temp": return temperature
        default: return pressure
        }
    }

    function eventColor(kind) {
        switch (kind) {
        case "score": return score1
        case "alarm": return score2
        case "prediction": return model
        case "technical": return faint
        case "registration": return "transparent"
        default: return text
        }
    }
}
