import QtQuick
import TokenMonitor

Rectangle {
    id: bubble
    radius: 8
    color: Theme.glass
    visible: false

    MouseArea {
        anchors.fill: parent
        onClicked: app.bubbleCollapsed = false
        drag.target: bubble
    }

    Text {
        anchors.centerIn: parent
        text: app.totalText
        color: Theme.number
        font.pixelSize: 13
        font.bold: true
        font.family: Theme.fontFamily
    }
}
