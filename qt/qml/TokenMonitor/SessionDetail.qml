import QtQuick
import QtQuick.Layouts
import TokenMonitor

Item {
    id: overlay
    visible: app.sessionDetailOpen

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
        MouseArea { anchors.fill: parent; onClicked: app.closeSession() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width - 16
        height: Math.min(parent.height - 24, 280)
        radius: Theme.radius
        color: Theme.panel
        border.color: Theme.line

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8
            RowLayout {
                width: parent.width
                Text {
                    text: app.sessionDetail.label || ""
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: true
                    font.family: Theme.fontFamily
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: "×"
                    color: Theme.muted
                    font.pixelSize: 16
                    MouseArea { anchors.fill: parent; onClicked: app.closeSession() }
                }
            }
            Text { text: (app.sessionDetail.client || "") + "  " + (app.sessionDetail.extra || ""); color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
            Text { text: app.formatTokens(app.sessionDetail.tokens || 0); color: Theme.number; font.pixelSize: 22; font.bold: true; font.family: Theme.fontFamily }
            Text { text: app.formatUsd(Number(app.sessionDetail.cost || 0)); color: Theme.muted; font.pixelSize: 12 }
        }
    }
}
