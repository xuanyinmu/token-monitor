import QtQuick
import TokenMonitor

Item {
    id: root
    property bool checked: false
    signal toggled(bool checked)
    implicitWidth: 34
    implicitHeight: 20
    width: 34
    height: 20

    Rectangle {
        anchors.fill: parent
        radius: 999
        color: root.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.88) : Theme.sunken
        border.width: 1
        border.color: root.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.16)
        Behavior on color { ColorAnimation { duration: 180 } }

        Rectangle {
            id: thumb
            width: 14
            height: 14
            radius: 7
            y: 2
            x: root.checked ? parent.width - 16 : 2
            color: root.checked ? Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1) : Theme.muted
            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
