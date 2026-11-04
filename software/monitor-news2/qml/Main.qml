import QtQuick
import QtQuick.Window
import MonitorNews2

// Tela de 7" do equipamento: 1024 × 600 fixos, paisagem, toque capacitivo.
Window {
    id: window

    required property bool isFullScreen
    property string screen: "monitor"
    property bool isHelpOpen: false

    function goTo(target) {
        isHelpOpen = false
        if (target === "register")
            registerScreen.reset()
        screen = target
    }

    width: 1024
    height: 600
    minimumWidth: 1024
    minimumHeight: 600
    maximumWidth: isFullScreen ? 16384 : 1024
    maximumHeight: isFullScreen ? 16384 : 600
    visibility: isFullScreen ? Window.FullScreen : Window.Windowed
    visible: true
    color: Theme.background
    title: "Monitor NEWS2 · Leito " + Monitor.patient.bed

    Item {
        id: device
        anchors.centerIn: parent
        width: 1024
        height: 600
        clip: true

        TopBar {
            id: topBar
            width: parent.width
        }

        Item {
            anchors.top: topBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            MonitorScreen {
                anchors.fill: parent
                visible: window.screen === "monitor"
                onRegisterRequested: window.goTo("register")
                onTrendsRequested: window.goTo("trends")
                onHandoffRequested: window.goTo("handoff")
                onHelpRequested: window.isHelpOpen = true
            }

            RegisterScreen {
                id: registerScreen
                anchors.fill: parent
                visible: window.screen === "register"
                onBackRequested: window.goTo("monitor")
            }

            TrendsScreen {
                anchors.fill: parent
                visible: window.screen === "trends"
                onBackRequested: window.goTo("monitor")
            }

            HandoffScreen {
                anchors.fill: parent
                visible: window.screen === "handoff"
                onBackRequested: window.goTo("monitor")
            }

            PredictionHelpModal {
                anchors.fill: parent
                visible: window.isHelpOpen && window.screen === "monitor"
                onCloseRequested: window.isHelpOpen = false
            }
        }

        Rectangle {
            readonly property string toastText: Monitor.display.toastText
            anchors.horizontalCenter: parent.horizontalCenter
            y: 60
            height: 48
            width: toastLabel.implicitWidth + 40
            visible: toastText.length > 0
            color: Theme.text

            UiText {
                id: toastLabel
                anchors.centerIn: parent
                text: parent.toastText
                font.pixelSize: 19
                font.weight: Font.DemiBold
                color: Theme.ink
            }
        }

        // Moldura de alarme em ≥ 5 (laranja) e ≥ 7 (vermelho).
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            visible: Monitor.alarm.frameTone !== ""
            border.width: 4
            border.color: Theme.tone(Monitor.alarm.frameTone)
        }

        // No equipamento, o modo noite reduz o backlight por PWM; aqui é simulado com
        // sobreposição. Alarme ≥ 5 força brilho total; toque acorda por 30 s.
        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: Monitor.display.isNightDimmed ? 0.6 : 0
        }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onPressed: mouse => {
                Monitor.wake()
                mouse.accepted = false
            }
        }
    }
}
