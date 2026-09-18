pragma Singleton
import QtQuick

QtObject {
    id: root
    property color accent: app && app.theme.accent ? app.theme.accent : "#b7ead4"
    property color bg: app && app.theme.bg ? app.theme.bg : "#303438"
    property color text: app && app.theme.text ? app.theme.text : "#eef5fb"
    property color muted: app && app.theme.muted ? app.theme.muted : "#a3adbb"
    property color number: app && app.theme.number ? app.theme.number : "#f3fbf7"
    property color line: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.138)
    property color lineStrong: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.238)
    property color overlay: Qt.rgba(1, 1, 1, 0.05)
    property color panel: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.55)
    property color sunken: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.55)
    property color red: "#f47788"
    property color orange: "#f4a073"
    property color blue: "#73bdf5"
    property color purple: "#b394f4"
    property color yellow: "#f1d973"
    property color shadow: Qt.rgba(0, 0, 0, 0.36)
    property color liveOff: "#5b6471"
    property real radius: 8
    property real controlRadius: 7
    property real controlAlpha: 0.049
    property real glassOpacity: app && app.theme.glassOpacity !== undefined ? app.theme.glassOpacity : 0.68
    property color glass: Qt.rgba(bg.r, bg.g, bg.b, glassOpacity)
    property color controlFill: Qt.rgba(1, 1, 1, controlAlpha)
    property string fontFamily: (app && app.theme.font) ? app.theme.font : "Cascadia Mono"
    property string displayFont: (app && app.theme.displayFont) ? app.theme.displayFont : "Segoe UI"
    property int bodySize: 11
}
