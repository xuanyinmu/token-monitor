import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TokenMonitor

Item {
    id: footer
    height: 30

    readonly property bool rateChipVisible: !app.settingsOpen && app.showLiveTokenRate && !app.appUpdateReady
    readonly property int switcherMax: {
        if (app.showLiveTokenRate)
            return Math.min(112, Math.max(80, Math.floor(width * 0.5 - 66)))
        return Math.min(150, Math.max(80, Math.floor(width * 0.52)))
    }

    RowLayout {
        anchors.fill: parent
        spacing: 8

        Item {
            id: switcher
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            implicitWidth: Math.min(footer.switcherMax, switcherBg.implicitWidth)
            Layout.preferredWidth: implicitWidth
            Layout.maximumWidth: footer.switcherMax
            Layout.minimumWidth: 0
            Layout.fillWidth: false
            Layout.preferredHeight: 30
            visible: true

            Rectangle {
                id: switcherBg
                implicitWidth: 8 + 14 + 6 + switcherLabel.implicitWidth + 8 + 24
                width: implicitWidth
                height: 30
                radius: 7
                color: (currentHover.containsMouse || switcherHover.containsMouse || menu.visible)
                       ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                       : Theme.controlFill
                border.width: 1
                border.color: (currentHover.containsMouse || switcherHover.containsMouse || menu.visible)
                              ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                              : Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.18)

                Row {
                    anchors.fill: parent
                    spacing: 0
                    Item {
                        id: currentBtn
                        width: parent.width - 24
                        height: 30
                        implicitWidth: 8 + 14 + 6 + switcherLabel.implicitWidth + 8
                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            TintIcon {
                                width: 14
                                height: 14
                                source: app.viewIcon
                                tint: Theme.muted
                                size: 14
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                id: switcherLabel
                                text: app.viewLabel
                                color: Theme.muted
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            id: currentHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            pressAndHoldInterval: 420
                            onClicked: app.cycleView()
                            onPressAndHold: menu.visible = true
                        }
                    }
                    Rectangle {
                        width: 1
                        height: parent.height
                        color: switcherBg.border.color
                    }
                    Item {
                        id: disclosureBtn
                        width: 23
                        height: 30
                        Canvas {
                            id: chevron
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = Theme.muted
                                ctx.lineWidth = 1.5
                                ctx.beginPath()
                                if (menu.visible) {
                                    ctx.moveTo(1.5, 5)
                                    ctx.lineTo(4, 2)
                                    ctx.lineTo(6.5, 5)
                                } else {
                                    ctx.moveTo(1.5, 2.5)
                                    ctx.lineTo(4, 5.5)
                                    ctx.lineTo(6.5, 2.5)
                                }
                                ctx.stroke()
                            }
                        }
                        Connections {
                            target: menu
                            function onVisibleChanged() { chevron.requestPaint() }
                        }
                        MouseArea {
                            id: switcherHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: menu.visible = !menu.visible
                        }
                    }
                }
            }
            Popup {
                id: menu
                y: -implicitHeight - 6
                width: 168
                padding: 6
                background: Rectangle {
                    radius: 10
                    color: Theme.glass
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                }
                Column {
                    spacing: 2
                    Repeater {
                        model: app.views
                        delegate: Rectangle {
                            width: 156
                            height: 28
                            radius: 6
                            color: modelData === app.view ? Qt.rgba(0.72, 0.92, 0.83, 0.12) : "transparent"
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                spacing: 8
                                TintIcon { width: 14; height: 14; size: 14; source: app.viewIconFor(modelData); tint: Theme.muted }
                                Text { text: app.viewLabelFor(modelData); color: Theme.text; font.pixelSize: 11; font.family: Theme.fontFamily }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { app.setView(modelData); menu.visible = false } }
                        }
                    }
                }
            }
        }

        Item {
            id: center
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.minimumWidth: 0

            Rectangle {
                visible: !app.settingsOpen && app.appUpdateReady
                anchors.centerIn: parent
                height: 22
                width: pillText.width + 28
                radius: 999
                color: Qt.rgba(0.72, 0.92, 0.83, 0.16)
                border.color: Qt.rgba(0.72, 0.92, 0.83, 0.45)
                Text {
                    id: pillText
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: -6
                    text: app.appUpdateLabel
                    color: Theme.accent
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×"
                    color: Theme.accent
                    font.pixelSize: 11
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: (mouse) => {
                        if (mouse.x > parent.width - 18) app.dismissUpdate()
                        else app.openUpdate()
                    }
                }
            }

            Row {
                visible: footer.rateChipVisible
                anchors.centerIn: parent
                spacing: 5
                TintIcon { width: 12; height: 12; size: 12; source: app.uiIcon("actions/zap.svg"); tint: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "tok/s"
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item {
            id: utility
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.preferredWidth: 34
            Layout.maximumWidth: 34
            Layout.minimumWidth: 34
            Layout.fillWidth: false
            Layout.preferredHeight: 30
            readonly property bool swapped: !!app.settings.settingsInTitlebar
            readonly property bool showSecondary: app.settingsOpen || utilHover.containsMouse || refreshBtn.hovered || settingsBtn.hovered

            IconButton {
                id: refreshBtn
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: utility.swapped ? undefined : parent.left
                anchors.rightMargin: utility.swapped ? 0 : 6
                anchors.horizontalCenter: utility.swapped ? parent.horizontalCenter : undefined
                buttonWidth: 34
                buttonHeight: 30
                opacity: (utility.swapped || utility.showSecondary) ? 1 : 0
                enabled: opacity > 0.5
                glyph: app.refreshing ? "" : "↻"
                iconSource: app.refreshing ? app.uiIcon("actions/spinner.svg") : ""
                active: app.refreshing
                spinning: app.refreshing
                onClicked: app.refresh()
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
            IconButton {
                id: settingsBtn
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: utility.swapped ? undefined : parent.horizontalCenter
                anchors.right: utility.swapped ? parent.left : undefined
                anchors.rightMargin: utility.swapped ? 6 : 0
                buttonWidth: 34
                buttonHeight: 30
                opacity: (!utility.swapped || utility.showSecondary) ? 1 : 0
                enabled: opacity > 0.5
                iconSource: app.uiIcon("actions/settings.svg")
                active: app.settingsOpen
                onClicked: app.toggleSettings()
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
            MouseArea {
                id: utilHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                z: -1
            }
        }
    }
}
