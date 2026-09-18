import QtQuick
import TokenMonitor

Rectangle {
    id: btn
    property alias glyph: label.text
    property url iconSource: ""
    property int iconSize: 13
    property bool active: false
    property bool spinning: false
    property alias hovered: ma.containsMouse
    property int buttonWidth: 34
    property int buttonHeight: 28
    signal clicked()

    width: buttonWidth
    height: buttonHeight
    radius: Theme.controlRadius
    color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : Theme.controlFill
    border.width: 1
    border.color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.58) : Theme.line

    TintIcon {
        id: glyphIcon
        anchors.centerIn: parent
        visible: btn.iconSource.toString().length > 0
        source: btn.iconSource
        size: btn.iconSize
        tint: btn.active ? Theme.accent : Theme.text
        rotation: 0
    }
    Text {
        id: label
        anchors.centerIn: parent
        visible: btn.iconSource.toString().length === 0
        color: btn.active ? Theme.accent : Theme.text
        font.pixelSize: 16
        font.family: Theme.fontFamily
    }
    RotationAnimator {
        target: glyphIcon
        from: 0
        to: 360
        duration: 800
        loops: Animation.Infinite
        running: btn.spinning
        onStopped: glyphIcon.rotation = 0
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
        onEntered: if (!btn.active) btn.color = Qt.rgba(1, 1, 1, 0.072)
        onExited: btn.color = btn.active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : Theme.controlFill
    }
}
