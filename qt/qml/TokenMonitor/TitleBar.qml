import QtQuick
import QtQuick.Layouts
import TokenMonitor

Item {
    id: bar
    height: statusLine.visible ? 44 : 30

    readonly property bool wantActions: app.settingsOpen || hotspotHot || actionsHover.containsMouse
    property bool revealed: app.settingsOpen
    property bool hotspotHot: false

    onWantActionsChanged: {
        if (wantActions) {
            hideTimer.stop()
            revealed = true
        } else {
            hideTimer.restart()
        }
    }

    Timer {
        id: hideTimer
        interval: 140
        onTriggered: if (!bar.wantActions) bar.revealed = false
    }

    MouseArea {
        anchors.fill: parent
        z: 0
        onPressed: app.startMove()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12
        z: 1

        Column {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            Row {
                spacing: 5
                Text {
                    visible: app.titleIconOnly
                    text: "Σ"
                    color: Theme.text
                    font.pixelSize: 16
                    font.bold: true
                    font.family: Theme.fontFamily
                }
                Text {
                    visible: !app.titleIconOnly
                    text: {
                        var s = app.i18n.t("app.name")
                        return (!s || s === "app.name") ? "Token Monitor" : s
                    }
                    color: Theme.text
                    font.pixelSize: 13
                    font.family: Theme.fontFamily
                    font.bold: true
                }
                Rectangle {
                    visible: app.settings.showLiveDot !== false && app.live
                    width: 4
                    height: 4
                    radius: 2
                    color: Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                id: statusLine
                text: app.statusVisible ? app.statusText : ""
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                visible: app.statusVisible && text.length > 0
            }
        }

        Item {
            id: controls
            Layout.preferredWidth: 148
            Layout.maximumWidth: 148
            Layout.minimumWidth: 100
            Layout.preferredHeight: 30
            Layout.alignment: Qt.AlignTop | Qt.AlignRight

            PeriodTabs {
                anchors.fill: parent
                z: 3
                opacity: bar.revealed ? 0 : 1
                enabled: !bar.revealed
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }

            // Electron .actions-hotspot: 28×28 at top:-12 right:-14, L clip-path
            // that lives in the 12/14px shell padding — not over MONTH/TOTAL.
            Item {
                id: hotspot
                z: 6
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: -12
                anchors.rightMargin: -14
                width: 28
                height: 28
                enabled: !app.settingsOpen
                clip: false

                // Top arm: full 28px × 12px, entirely above the tabs.
                MouseArea {
                    x: 0
                    y: 0
                    width: 28
                    height: 12
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onEntered: bar.hotspotHot = true
                    onExited: Qt.callLater(hotspot.recompute)
                }
                // Right arm: 14px × 28px, entirely in the window's right padding.
                MouseArea {
                    x: 14
                    y: 0
                    width: 14
                    height: 28
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onEntered: bar.hotspotHot = true
                    onExited: Qt.callLater(hotspot.recompute)
                }

                function recompute() {
                    bar.hotspotHot = children[0].containsMouse || children[1].containsMouse
                }
            }

            MouseArea {
                id: actionsHover
                z: 5
                anchors.right: parent.right
                anchors.top: parent.top
                width: 114
                height: 30
                hoverEnabled: bar.revealed
                enabled: bar.revealed
                acceptedButtons: Qt.NoButton
            }

            Row {
                id: actions
                z: 4
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 6
                height: 30
                opacity: bar.revealed ? 1 : 0
                visible: bar.revealed || fade.running
                enabled: bar.revealed
                Behavior on opacity { NumberAnimation { id: fade; duration: 120 } }

                IconButton {
                    buttonWidth: 34
                    buttonHeight: 28
                    glyph: "⇧"
                    onClicked: app.cycleBehavior()
                }
                IconButton {
                    buttonWidth: 34
                    buttonHeight: 28
                    glyph: "−"
                    onClicked: app.minimizeWindow()
                }
                IconButton {
                    buttonWidth: 34
                    buttonHeight: 28
                    glyph: "×"
                    onClicked: app.closeWindow()
                }
            }
        }
    }
}
