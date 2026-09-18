import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TokenMonitor

Window {
    id: dash
    width: 920
    height: 620
    minimumWidth: 560
    minimumHeight: 420
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    title: dash.tr("dashboard.title", "Usage Dashboard")

    property string tab: "activity"
    property string heatMetric: "cost"
    property string stackBy: "client"
    property string chartMode: "bars"
    property int rangeDays: 30

    function tr(key, fallback) {
        var s = app.i18n.t(key)
        return (!s || s === key) ? fallback : s
    }

    function shortDate(raw) {
        var m = /^(\d{4})-(\d{2})-(\d{2})/.exec(String(raw || ""))
        if (m) return Number(m[2]) + "/" + Number(m[3])
        return String(raw || "")
    }

    function cardValue(key) {
        var s = app.dashboardSummary || {}
        if (key === "totalTokens") return app.formatTokens(Number(s.totalTokens || 0))
        if (key === "totalCost") return app.formatUsd(Number(s.totalCost || 0))
        if (key === "activeDays") return String(s.activeDays || 0)
        if (key === "currentStreak") return String(s.currentStreak || 0)
        if (key === "activeTimeMs") return app.formatDuration(Number(s.activeTimeMs || 0))
        if (key === "peakDayTokens") return app.formatTokens(Number(s.peakDayTokens || 0))
        if (key === "favoriteModel") return s.favoriteModel || "—"
        if (key === "messages") return app.formatNumber(Number(s.messages || 0))
        return "—"
    }

    function cardLabel(key) {
        if (key === "totalTokens") return dash.tr("dashboard.stat.totalTokens", "Total tokens")
        if (key === "totalCost") return dash.tr("dashboard.stat.totalCost", "Total cost")
        if (key === "activeDays") return dash.tr("trends.activeDays", "Active days")
        if (key === "currentStreak") return dash.tr("trends.currentStreak", "Current streak")
        if (key === "activeTimeMs") return dash.tr("trends.activeTime", "Active time")
        if (key === "peakDayTokens") return dash.tr("trends.peakDay", "Peak day")
        if (key === "favoriteModel") return dash.tr("dashboard.stat.favoriteModel", "Top model")
        return dash.tr("dashboard.stat.messages", "Messages")
    }

    function rangePoints() {
        var days = app.historyDays || []
        var n = dash.rangeDays
        if (n <= 0 || days.length <= n) return days
        return days.slice(days.length - n)
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.glass
    }

    Item {
        width: parent.width - 140
        height: 42
        z: 2
        DragHandler {
            target: null
            onActiveChanged: if (active) dash.startSystemMove()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                Text {
                    text: dash.tr("dashboard.title", "Usage Dashboard")
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    font.letterSpacing: 0.3
                    Layout.fillWidth: true
                }
                Repeater {
                    model: [
                        { glyph: "◐", action: "theme" },
                        { glyph: "↻", action: "refresh" },
                        { glyph: "−", action: "min" },
                        { glyph: "×", action: "close" }
                    ]
                    delegate: Rectangle {
                        width: 26
                        height: 26
                        radius: 6
                        color: Qt.rgba(1, 1, 1, headerHover.hovered ? 0.12 : 0.05)
                        Text {
                            anchors.centerIn: parent
                            text: modelData.glyph
                            color: Theme.text
                            font.pixelSize: 14
                            font.family: Theme.fontFamily
                        }
                        HoverHandler { id: headerHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData.action === "refresh") app.refresh()
                                else if (modelData.action === "min") dash.showMinimized()
                                else if (modelData.action === "close") dash.hide()
                            }
                        }
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }
        }

        Row {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 10
            spacing: 4
            Repeater {
                model: [
                    { id: "activity", key: "dashboard.tab.activity", fallback: "Overview" },
                    { id: "trends", key: "dashboard.tab.trends", fallback: "Trends" }
                ]
                delegate: Rectangle {
                    width: tabLab.implicitWidth + 24
                    height: 28
                    radius: 7
                    color: dash.tab === modelData.id ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                    Text {
                        id: tabLab
                        anchors.centerIn: parent
                        text: dash.tr(modelData.key, modelData.fallback)
                        color: Theme.text
                        opacity: dash.tab === modelData.id ? 1 : 0.55
                        font.pixelSize: 12
                        font.family: Theme.fontFamily
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dash.tab = modelData.id
                    }
                }
            }
        }

        Flickable {
            visible: dash.tab === "activity"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: overviewCol.height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: overviewCol
                width: parent.width - 28
                x: 14
                spacing: 12

                Rectangle {
                    width: parent.width
                    height: 78
                    radius: 12
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: ["totalTokens", "totalCost", "activeDays", "currentStreak", "activeTimeMs", "peakDayTokens", "favoriteModel", "messages"]
                            delegate: Item {
                                width: parent.width / 8
                                height: parent.height
                                Rectangle {
                                    visible: index < 7
                                    anchors.right: parent.right
                                    width: 1
                                    height: parent.height
                                    color: Qt.rgba(1, 1, 1, 0.06)
                                }
                                Column {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    anchors.topMargin: 14
                                    spacing: 5
                                    Text {
                                        width: parent.width
                                        text: dash.cardValue(modelData)
                                        color: Theme.number
                                        font.pixelSize: 19
                                        font.weight: Font.DemiBold
                                        font.family: Theme.displayFont
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: dash.cardLabel(modelData)
                                        color: Theme.muted
                                        font.pixelSize: 10
                                        font.family: Theme.fontFamily
                                        font.letterSpacing: 0.4
                                        font.capitalization: Font.AllUppercase
                                        opacity: 0.55
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 28
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: dash.tr("dashboard.heatmap.title", "Token Activity")
                        color: Theme.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        opacity: 0.9
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: tokBtn.width + costBtn.width + 8
                        height: 24
                        radius: 7
                        color: Qt.rgba(1, 1, 1, 0.05)
                        Row {
                            anchors.centerIn: parent
                            spacing: 2
                            Rectangle {
                                id: tokBtn
                                width: tokLab.implicitWidth + 16
                                height: 20
                                radius: 5
                                color: dash.heatMetric === "tokens" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                                Text {
                                    id: tokLab
                                    anchors.centerIn: parent
                                    text: dash.tr("dashboard.heatmap.tokens", "Tokens")
                                    color: Theme.text
                                    opacity: dash.heatMetric === "tokens" ? 1 : 0.6
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dash.heatMetric = "tokens"
                                }
                            }
                            Rectangle {
                                id: costBtn
                                width: costLab.implicitWidth + 16
                                height: 20
                                radius: 5
                                color: dash.heatMetric === "cost" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                                Text {
                                    id: costLab
                                    anchors.centerIn: parent
                                    text: dash.tr("dashboard.heatmap.cost", "Cost")
                                    color: Theme.text
                                    opacity: dash.heatMetric === "cost" ? 1 : 0.6
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dash.heatMetric = "cost"
                                }
                            }
                        }
                    }
                }

                Heatmap {
                    width: parent.width
                    height: 140
                    cell: 14
                    gap: 4
                    metric: dash.heatMetric
                    twelveMonth: true
                    days: app.historyDays
                }

                Row {
                    width: parent.width
                    spacing: 32
                    Repeater {
                        model: [
                            { titleKey: "dashboard.stack.model", fallback: "By model", rows: "models" },
                            { titleKey: "dashboard.stack.client", fallback: "By tool", rows: "tools" }
                        ]
                        delegate: Column {
                            width: (parent.width - 32) / 2
                            spacing: 6
                            Text {
                                text: dash.tr(modelData.titleKey, modelData.fallback)
                                color: Theme.text
                                opacity: 0.6
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                font.letterSpacing: 0.5
                                font.capitalization: Font.AllUppercase
                            }
                            Repeater {
                                model: {
                                    var src = modelData.rows === "models" ? (app.dashboardModelRows || []) : (app.dashboardToolRows || [])
                                    return src.slice(0, 5)
                                }
                                delegate: RowLayout {
                                    width: parent.width
                                    spacing: 12
                                    height: 18
                                    Item {
                                        Layout.preferredWidth: 96
                                        Layout.maximumWidth: 96
                                        height: parent.height
                                        Row {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 8
                                            Rectangle {
                                                width: 11
                                                height: 11
                                                radius: 3
                                                color: modelData.color || Theme.blue
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                width: 76
                                                text: modelData.label
                                                color: Theme.text
                                                opacity: 0.9
                                                font.pixelSize: 12
                                                font.family: Theme.fontFamily
                                                elide: Text.ElideRight
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 40
                                        height: 4
                                        radius: 2
                                        color: Qt.rgba(1, 1, 1, 0.06)
                                        Rectangle {
                                            width: parent.width * Math.max(0, Math.min(1, Number(modelData.percent || 0)))
                                            height: parent.height
                                            radius: 2
                                            color: modelData.color || Theme.blue
                                        }
                                    }
                                    Text {
                                        Layout.preferredWidth: 52
                                        Layout.maximumWidth: 52
                                        text: app.formatTokens(Number(modelData.tokens || 0))
                                        color: Theme.text
                                        opacity: 0.8
                                        font.pixelSize: 12
                                        font.family: Theme.fontFamily
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.preferredWidth: 44
                                        Layout.maximumWidth: 44
                                        text: Math.round(Number(modelData.percent || 0) * 100) + "%"
                                        color: Theme.muted
                                        opacity: 0.55
                                        font.pixelSize: 12
                                        font.family: Theme.fontFamily
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: dash.tab === "trends"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 14
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Rectangle {
                    height: 28
                    width: clientSeg.width + modelSeg.width + 8
                    radius: 7
                    color: Qt.rgba(1, 1, 1, 0.05)
                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        Rectangle {
                            id: clientSeg
                            width: clientSegLab.implicitWidth + 20
                            height: 22
                            radius: 5
                            color: dash.stackBy === "client" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            Text {
                                id: clientSegLab
                                anchors.centerIn: parent
                                text: dash.tr("dashboard.stack.client", "By tool")
                                color: Theme.text
                                opacity: dash.stackBy === "client" ? 1 : 0.6
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dash.stackBy = "client" }
                        }
                        Rectangle {
                            id: modelSeg
                            width: modelSegLab.implicitWidth + 20
                            height: 22
                            radius: 5
                            color: dash.stackBy === "model" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            Text {
                                id: modelSegLab
                                anchors.centerIn: parent
                                text: dash.tr("dashboard.stack.model", "By model")
                                color: Theme.text
                                opacity: dash.stackBy === "model" ? 1 : 0.6
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dash.stackBy = "model" }
                        }
                    }
                }
                Rectangle {
                    height: 28
                    width: barsSeg.width + klineSeg.width + 8
                    radius: 7
                    color: Qt.rgba(1, 1, 1, 0.05)
                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        Rectangle {
                            id: barsSeg
                            width: barsLab.implicitWidth + 20
                            height: 22
                            radius: 5
                            color: dash.chartMode === "bars" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            Text {
                                id: barsLab
                                anchors.centerIn: parent
                                text: dash.tr("dashboard.mode.bars", "Bars")
                                color: Theme.text
                                opacity: dash.chartMode === "bars" ? 1 : 0.6
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dash.chartMode = "bars" }
                        }
                        Rectangle {
                            id: klineSeg
                            width: klineLab.implicitWidth + 20
                            height: 22
                            radius: 5
                            color: dash.chartMode === "kline" ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            Text {
                                id: klineLab
                                anchors.centerIn: parent
                                text: dash.tr("dashboard.mode.kline", "K-line")
                                color: Theme.text
                                opacity: dash.chartMode === "kline" ? 1 : 0.6
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dash.chartMode = "kline" }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Repeater {
                    model: [
                        { days: 7, label: "7D" },
                        { days: 30, label: "30D" },
                        { days: 90, label: "90D" },
                        { days: 365, label: "1Y" }
                    ]
                    delegate: Text {
                        text: modelData.label
                        color: Theme.text
                        opacity: dash.rangeDays === modelData.days ? 1 : 0.65
                        font.pixelSize: 12
                        font.family: Theme.fontFamily
                        leftPadding: 10
                        rightPadding: 10
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dash.rangeDays = modelData.days
                        }
                    }
                }
            }

            Canvas {
                id: trendCanvas
                Layout.fillWidth: true
                Layout.fillHeight: true
                property var points: dash.rangePoints()
                onPointsChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: app
                    function onStatsChanged() {
                        trendCanvas.points = dash.rangePoints()
                        trendCanvas.requestPaint()
                    }
                }
                Connections {
                    target: dash
                    function onRangeDaysChanged() {
                        trendCanvas.points = dash.rangePoints()
                        trendCanvas.requestPaint()
                    }
                    function onChartModeChanged() { trendCanvas.requestPaint() }
                }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var pts = trendCanvas.points || []
                    if (!pts.length) return
                    var padL = 8, padR = 8, padT = 8, padB = 22
                    var innerW = Math.max(1, width - padL - padR)
                    var innerH = Math.max(1, height - padT - padB)
                    var maxVal = 1
                    for (var i = 0; i < pts.length; ++i)
                        maxVal = Math.max(maxVal, Number(pts[i].tokens || 0))
                    ctx.strokeStyle = Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.07)
                    ctx.lineWidth = 1
                    for (var g = 0; g < 4; ++g) {
                        var gy = padT + innerH * g / 3
                        ctx.beginPath()
                        ctx.moveTo(padL, gy)
                        ctx.lineTo(padL + innerW, gy)
                        ctx.stroke()
                    }
                    ctx.strokeStyle = Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.18)
                    ctx.beginPath()
                    ctx.moveTo(padL, padT + innerH)
                    ctx.lineTo(padL + innerW, padT + innerH)
                    ctx.stroke()
                    if (dash.chartMode === "kline") {
                        var slot = innerW / pts.length
                        for (var k = 0; k < pts.length; ++k) {
                            var cur = Number(pts[k].tokens || 0)
                            var prev = k > 0 ? Number(pts[k - 1].tokens || 0) : cur
                            var high = Math.max(cur, prev)
                            var low = Math.min(cur, prev)
                            var x = padL + slot * k + slot / 2
                            var y1 = padT + innerH - innerH * high / maxVal
                            var y2 = padT + innerH - innerH * low / maxVal
                            var bodyH = Math.max(2, Math.abs(y2 - y1))
                            ctx.strokeStyle = cur >= prev ? "#4ec77f" : "#f06a7b"
                            ctx.fillStyle = ctx.strokeStyle
                            ctx.beginPath()
                            ctx.moveTo(x, y1)
                            ctx.lineTo(x, y2)
                            ctx.stroke()
                            ctx.fillRect(x - Math.max(2, slot * 0.22), Math.min(y1, y2), Math.max(4, slot * 0.44), bodyH)
                        }
                    } else {
                        var slotB = innerW / pts.length
                        var barW = Math.max(2, slotB * 0.62)
                        ctx.fillStyle = "#73bdf5"
                        for (var b = 0; b < pts.length; ++b) {
                            var val = Number(pts[b].tokens || 0)
                            var bh = innerH * val / maxVal
                            var bx = padL + slotB * b + (slotB - barW) / 2
                            ctx.globalAlpha = b === pts.length - 1 ? 1 : 0.55
                            ctx.fillRect(bx, padT + innerH - bh, barW, Math.max(1, bh))
                        }
                        ctx.globalAlpha = 1
                    }
                    ctx.fillStyle = Theme.muted
                    ctx.font = "10px " + Theme.fontFamily
                    ctx.fillText(dash.shortDate(pts[0].date), padL, height - 6)
                    ctx.fillText(dash.shortDate(pts[pts.length - 1].date), padL + innerW - 36, height - 6)
                }
            }

            Column {
                Layout.fillWidth: true
                Layout.maximumHeight: 136
                spacing: 1
                Repeater {
                    model: {
                        var src = dash.stackBy === "model" ? (app.dashboardModelRows || []) : (app.dashboardToolRows || [])
                        return src.slice(0, 8)
                    }
                    delegate: Row {
                        width: parent.width
                        height: 22
                        spacing: 12
                        Row {
                            width: parent.width - 160
                            spacing: 8
                            Rectangle {
                                width: 11
                                height: 11
                                radius: 3
                                color: modelData.color || Theme.blue
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.label
                                color: Theme.text
                                opacity: 0.9
                                font.pixelSize: 12
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                                width: parent.width - 20
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Text {
                            width: 72
                            text: app.formatTokens(Number(modelData.tokens || 0))
                            color: Theme.text
                            opacity: 0.8
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            width: 50
                            text: Math.round(Number(modelData.percent || 0) * 1000) / 10 + "%"
                            color: Theme.muted
                            opacity: 0.55
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
