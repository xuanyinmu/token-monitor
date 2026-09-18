import QtQuick
import QtQuick.Layouts
import TokenMonitor

Flickable {
    id: home
    clip: true
    contentWidth: width
    contentHeight: col.height
    boundsBehavior: Flickable.StopAtBounds
    boundsMovement: Flickable.StopAtBounds

    function tr(key, fallback) {
        var s = app.i18n.t(key)
        return (!s || s === key) ? fallback : s
    }

    function take(list, n) {
        var out = []
        if (!list) return out
        for (var i = 0; i < Math.min(n, list.length); ++i) out.push(list[i])
        return out
    }

    function configuredLimits() {
        var out = []
        var list = app.limitRows || []
        for (var i = 0; i < list.length; ++i) {
            var r = list[i]
            if (String(r.status || "") === "notConfigured") continue
            var windows = r.homeWindows || r.windows || []
            if (windows.length === 0 && !(r.percent > 0) && !r.detail) continue
            out.push(r)
        }
        return out
    }

    function remainFrac(v) {
        var n = Number(v)
        if (!isFinite(n)) return 0
        if (Math.abs(n) > 1) return Math.max(0, Math.min(1, n / 100))
        return Math.max(0, Math.min(1, n))
    }

    function shareText(row) {
        return Math.round(Number(row.percent || 0) * 100) + "%"
    }

    function peakText() {
        var peak = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i)
            peak = Math.max(peak, Number(days[i].tokens || 0))
        var value = app.formatTokens(peak)
        var s = app.i18n.t("home.peakTokens", { value: value })
        return (!s || s === "home.peakTokens") ? ("Peak " + value) : s
    }

    function activeDays() {
        var n = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i) {
            if (Number(days[i].tokens || 0) > 0) n++
        }
        if (!n) return ""
        var s = app.i18n.t("home.activeDays", { count: n })
        return (!s || s === "home.activeDays") ? (n + " days") : s
    }

    Column {
        id: col
        width: home.width
        spacing: 12

        Repeater {
            model: app.homeModules
            delegate: Loader {
                width: col.width
                sourceComponent: {
                    switch (String(modelData)) {
                    case "limits": return limitsComp
                    case "tool":
                    case "tools": return toolsComp
                    case "device": return deviceComp
                    case "model":
                    case "models": return modelsComp
                    case "trends":
                    case "heatmap": return heatComp
                    default: return emptyComp
                    }
                }
            }
        }
    }

    component ModuleHead: Item {
        id: head
        property string title: ""
        property string meta: ""
        property url jumpIcon: ""
        property string jumpView: ""
        width: parent.width
        height: 18
        Text {
            text: head.title
            color: Theme.text
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            font.capitalization: Font.AllUppercase
        }
        Row {
            anchors.right: parent.right
            spacing: 7
            Text {
                visible: head.meta.length > 0
                text: head.meta
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                anchors.verticalCenter: parent.verticalCenter
            }
            TintIcon {
                visible: head.jumpIcon.toString().length > 0
                source: head.jumpIcon
                size: 13
                tint: Theme.muted
                opacity: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: if (head.jumpView.length) app.view = head.jumpView
        }
    }

    component ModuleRule: Rectangle {
        width: parent.width
        height: 1
        color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.12)
    }

    Component {
        id: limitsComp
        Column {
            width: col.width
            spacing: 0
            Column {
                width: col.width
                spacing: 7
                bottomPadding: 12
                ModuleHead {
                    title: home.tr("home.limits", "Limits")
                jumpView: "limits"
                jumpIcon: app.viewIconFor("limits")
            }
            Column {
                width: col.width
                spacing: 12
                Repeater {
                    model: home.take(home.configuredLimits(), 3)
                    Column {
                        id: accountCol
                    property var account: modelData
                    width: col.width
                    spacing: 4
                    Row {
                        spacing: 8
                        height: 14
                        TintIcon {
                            visible: (accountCol.account.icon || "").length > 0
                            source: accountCol.account.icon || ""
                            size: 10
                            tint: Theme.text
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: accountCol.account.label
                            color: Theme.text
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    RowLayout {
                        width: parent.width
                        spacing: 12
                        Item { Layout.preferredWidth: 18; Layout.maximumWidth: 18; Layout.minimumWidth: 18; height: 1 }
                        Repeater {
                            model: home.take(accountCol.account.homeWindows || accountCol.account.windows || [], 2)
                            Column {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                spacing: 4
                                Item {
                                    width: parent.width
                                    height: 14
                                    Text {
                                        anchors.left: parent.left
                                        anchors.right: winVal.left
                                        anchors.rightMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.homeLabel || modelData.label || modelData.kind
                                        color: Theme.muted
                                        font.pixelSize: 10
                                        font.family: Theme.fontFamily
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        id: winVal
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.value || ""
                                        color: {
                                            if (!app.settings.showHomeLimitBars) return Theme.text
                                            var rp = home.remainFrac(modelData.remainingPercent)
                                            return rp < 0.2 ? Theme.red : (rp < 0.5 ? Theme.yellow : Theme.text)
                                        }
                                        font.pixelSize: 10
                                        font.family: Theme.fontFamily
                                    }
                                }
                                Rectangle {
                                    visible: !!app.settings.showHomeLimitBars
                                    width: parent.width
                                    height: 6
                                    radius: 3
                                    color: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.46)
                                    Rectangle {
                                        width: parent.width * home.remainFrac(modelData.remainingPercent)
                                        height: parent.height
                                        radius: 3
                                        color: home.remainFrac(modelData.remainingPercent) < 0.2 ? Theme.red : Theme.blue
                                    }
                                }
                                Text {
                                    visible: (modelData.resetText || "").length > 0
                                    width: parent.width
                                    text: modelData.resetText || ""
                                    color: Theme.muted
                                    font.pixelSize: 9
                                    font.family: Theme.fontFamily
                                    opacity: 0.82
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                    }
                }
            }
            Text {
                visible: home.configuredLimits().length === 0
                text: home.tr("home.noLimits", "No live limit windows yet")
                color: Theme.muted
                font.pixelSize: 11
                font.family: Theme.fontFamily
            }
            }
            ModuleRule {}
        }
    }
    Component {
        id: toolsComp
        Column {
            width: col.width
            spacing: 0
            Column {
                width: col.width
                spacing: 7
                bottomPadding: 12
                ModuleHead {
                    title: home.tr("home.tools", "Tools")
                meta: app.clientRows.length ? (app.clientRows.length + "") : ""
                jumpView: "tool"
                jumpIcon: app.viewIconFor("tool")
            }
            Repeater {
                model: home.take(app.clientRows, 5)
                HomeListRow {
                    width: col.width
                    icon: modelData.icon || ""
                    label: modelData.label
                    value: app.formatTokens(modelData.tokens)
                    aux: home.shareText(modelData)
                }
            }
            }
            ModuleRule {}
        }
    }
    Component {
        id: deviceComp
        Column {
            width: col.width
            spacing: 0
            Column {
                width: col.width
                spacing: 7
                bottomPadding: 12
                ModuleHead {
                    title: home.tr("home.devices", "Devices")
                jumpView: "device"
                jumpIcon: app.viewIconFor("device")
            }
            Repeater {
                model: home.take(app.deviceRows, 4)
                HomeListRow {
                    width: col.width
                    label: modelData.label || modelData.id
                    value: app.formatTokens(modelData.tokens)
                    aux: ""
                    showAux: false
                }
            }
            }
            ModuleRule {}
        }
    }
    Component {
        id: modelsComp
        Column {
            width: col.width
            spacing: 0
            Column {
                width: col.width
                spacing: 7
                bottomPadding: 12
                ModuleHead {
                    title: home.tr("home.models", "Models")
                jumpView: "model"
                jumpIcon: app.viewIconFor("model")
            }
            Repeater {
                model: home.take(app.modelRows, 5)
                HomeListRow {
                    width: col.width
                    icon: modelData.icon || ""
                    markColor: modelData.color || Theme.blue
                    label: modelData.label
                    value: app.formatTokens(modelData.tokens)
                    aux: home.shareText(modelData)
                }
            }
            }
            ModuleRule {}
        }
    }
    Component {
        id: heatComp
        Column {
            width: col.width
            spacing: 0
            Column {
                width: col.width
                spacing: 7
                bottomPadding: 12
                ModuleHead {
                    title: home.tr("home.activity", "Activity")
                meta: home.activeDays()
                jumpView: "trends"
                jumpIcon: app.viewIconFor("trends")
            }
            Heatmap {
                width: col.width
                height: 102
                days: app.historyDays
                twelveMonth: true
            }
            Item {
                width: col.width
                height: 18
                Text {
                    text: home.tr("home.trend", "Trend")
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: home.peakText()
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                }
            }
            TrendChart {
                width: col.width
                height: 82
                points: app.trendPoints
            }
            }
            ModuleRule {}
        }
    }
    Component {
        id: emptyComp
        Item { width: 1; height: 0 }
    }
}
