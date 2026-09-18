import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TokenMonitor

Window {
    id: root
    width: 340
    height: 650
    minimumWidth: 240
    minimumHeight: 140
    maximumWidth: 1200
    maximumHeight: 1400
    visible: true
    color: Theme.bg
    flags: Qt.FramelessWindowHint | Qt.Window
    title: "Token Monitor"

    onWidthChanged: sizeSave.restart()
    onHeightChanged: sizeSave.restart()
    Timer {
        id: sizeSave
        interval: 250
        onTriggered: app.persistWindowSize(root.width, root.height)
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.glass
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 12
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 14
            spacing: 8

            TitleBar {
                Layout.fillWidth: true
            }

            Item {
                id: settingsSlot
                visible: app.settingsOpen
                z: 2
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: settingsPanel.implicitHeight
                Layout.maximumHeight: Math.max(80, root.height - 220)
                Layout.minimumWidth: 0
                Layout.minimumHeight: 0
                clip: true

                SettingsPanel {
                    id: settingsPanel
                    anchors.fill: parent
                }
            }

            TotalPanel {
                Layout.fillWidth: true
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                Layout.topMargin: -6
                Layout.bottomMargin: -2
                visible: !app.settingsOpen && app.view !== "home" && app.homeReturnVisible
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    TintIcon {
                        source: app.uiIcon("actions/arrow-left.svg")
                        size: 12
                        tint: Theme.muted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: app.i18n.t("views.backHome") === "views.backHome" ? "Back to Home" : app.i18n.t("views.backHome")
                        color: Theme.muted
                        font.pixelSize: 11
                        font.family: Theme.fontFamily
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: app.view = "home"
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                clip: true
                z: 0

                Loader {
                    anchors.fill: parent
                    width: parent.width
                    height: parent.height
                    onLoaded: {
                        if (item) {
                            item.width = Qt.binding(function() { return width })
                            item.height = Qt.binding(function() { return height })
                        }
                    }
                    sourceComponent: {
                        switch (app.view) {
                        case "tool": return toolComp
                        case "model": return modelComp
                        case "limits": return limitsComp
                        case "session": return sessionComp
                        case "project": return projectComp
                        case "device": return deviceComp
                        case "status": return statusComp
                        case "trends": return trendsComp
                        default: return homeComp
                        }
                    }
                }

                SessionDetail {
                    anchors.fill: parent
                    visible: app.sessionDetailOpen
                }
            }

            Footer {
                Layout.fillWidth: true
            }
        }
    }

    component ResizeEdge: MouseArea {
        required property string edge
        required property int cursor
        hoverEnabled: true
        cursorShape: cursor
        z: 100
        visible: (app.settings.windowBehavior || "floating") !== "desktop"
        onPressed: app.startResize(edge)
    }
    ResizeEdge { edge: "l"; cursor: Qt.SizeHorCursor; width: 6; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.topMargin: 8; anchors.bottomMargin: 8 }
    ResizeEdge { edge: "r"; cursor: Qt.SizeHorCursor; width: 6; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.topMargin: 8; anchors.bottomMargin: 8 }
    ResizeEdge { edge: "t"; cursor: Qt.SizeVerCursor; height: 6; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; anchors.leftMargin: 8; anchors.rightMargin: 8 }
    ResizeEdge { edge: "b"; cursor: Qt.SizeVerCursor; height: 6; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.leftMargin: 8; anchors.rightMargin: 8 }
    ResizeEdge { edge: "tl"; cursor: Qt.SizeFDiagCursor; width: 10; height: 10; anchors.left: parent.left; anchors.top: parent.top }
    ResizeEdge { edge: "tr"; cursor: Qt.SizeBDiagCursor; width: 10; height: 10; anchors.right: parent.right; anchors.top: parent.top }
    ResizeEdge { edge: "bl"; cursor: Qt.SizeBDiagCursor; width: 10; height: 10; anchors.left: parent.left; anchors.bottom: parent.bottom }
    ResizeEdge { edge: "br"; cursor: Qt.SizeFDiagCursor; width: 10; height: 10; anchors.right: parent.right; anchors.bottom: parent.bottom }

    Window {
        id: bubbleWin
        width: 56
        height: 56
        visible: app.floatingBubble
        color: "transparent"
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        title: "Token Monitor"
        x: root.x + root.width - 72
        y: root.y + 48

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: Theme.glass
            Text {
                anchors.centerIn: parent
                text: app.compactText
                color: Theme.number
                font.pixelSize: 13
                font.weight: Font.Bold
                font.family: Theme.fontFamily
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    app.showWindow()
                    app.view = "home"
                }
                onPressed: bubbleWin.startSystemMove()
            }
        }
    }

    Component { id: homeComp; HomeView {} }
    Component { id: toolComp; ToolView {} }
    Component { id: modelComp; ModelView {} }
    Component { id: limitsComp; LimitsView {} }
    Component { id: sessionComp; SessionView {} }
    Component { id: projectComp; ProjectView {} }
    Component { id: statusComp; StatusView {} }
    Component { id: deviceComp; DeviceView {} }
    Component { id: trendsComp; TrendsView {} }
}
