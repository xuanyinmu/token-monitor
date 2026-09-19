import QtQuick
import QtQuick.Controls
import TokenMonitor

Item {
    id: tabs
    width: 148
    height: 30
    property var items: [
        { id: "today", label: "DAY" },
        { id: "month", label: "MONTH" },
        { id: "allTime", label: "TOTAL" }
    ]

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: Qt.rgba(1, 1, 1, 0.026)
        border.width: 1
        border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.09)
    }

    Rectangle {
        id: indicator
        x: 3 + app.periodIndex * (slotWidth + 2)
        y: 3
        width: slotWidth
        height: parent.height - 6
        radius: 6
        color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.13)
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            radius: 6
            color: Qt.rgba(1, 1, 1, 0.075)
        }
        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    }

    readonly property real slotWidth: (width - 10) / 3

    Row {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 2
        Repeater {
            model: tabs.items
            delegate: Item {
                width: tabs.slotWidth
                height: 22
                Text {
                    anchors.centerIn: parent
                    // The middle slot reflects the selected month sub-range,
                    // like Electron fixedPeriodRanges.displayLabel (WEEK/7D/30D).
                    // Referencing periodMonthMode re-evaluates the label when
                    // the default-range preference changes.
                    text: {
                        var _ = app.settings.periodMonthMode
                        return modelData.id === "month" ? app.periodTabLabel : modelData.label
                    }
                    color: app.period === modelData.id || (modelData.id === "month" && app.periodIndex === 1)
                           ? Theme.accent : Theme.muted
                    font.pixelSize: 9
                    font.bold: true
                    font.family: Theme.fontFamily
                    font.letterSpacing: 0.4
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (modelData.id === "month" && (app.period === "month" || app.periodIndex === 1))
                            tabs.showMonthMenu()
                        else
                            app.period = modelData.id
                    }
                    acceptedButtons: Qt.LeftButton
                }
            }
        }
    }

    function showMonthMenu() {
        var overlay = Overlay.overlay
        monthMenu.parent = overlay ? overlay : tabs.Window.window.contentItem
        var p = tabs.mapToItem(monthMenu.parent, Math.max(0, tabs.width - monthMenu.width), tabs.height + 4)
        monthMenu.x = p.x
        monthMenu.y = p.y
        monthMenu.open()
    }

    Popup {
        id: monthMenu
        width: 132
        padding: 4
        modal: true
        dim: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        // Electron opens its period menu with a quick fade/slide from the
        // anchor; mirror that instead of popping in.
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 140; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.97; duration: 110; easing.type: Easing.OutCubic }
        }
        transformOrigin: Item.TopRight
        background: Rectangle {
            radius: 7
            color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0.96)
            border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.22)
            border.width: 1
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
            }
        }
        Column {
            width: parent.width
            spacing: 2
            Repeater {
                model: [
                    { id: "month", key: "periodRange.month", fallback: "This month" },
                    { id: "week", key: "periodRange.week", fallback: "This week" },
                    { id: "last7", key: "periodRange.last7", fallback: "Last 7 days" },
                    { id: "last30", key: "periodRange.last30", fallback: "Last 30 days" }
                ]
                delegate: Rectangle {
                    width: monthMenu.availableWidth
                    height: 28
                    radius: 5
                    color: app.period === modelData.id ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 7
                        text: {
                            var s = app.i18n.t(modelData.key)
                            return (!s || s === modelData.key) ? modelData.fallback : s
                        }
                        color: app.period === modelData.id ? Theme.accent : Theme.muted
                        font.pixelSize: 11
                        font.family: Theme.fontFamily
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            app.period = modelData.id
                            monthMenu.close()
                        }
                    }
                }
            }
        }
    }
}
