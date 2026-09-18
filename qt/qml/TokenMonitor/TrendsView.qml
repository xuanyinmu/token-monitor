import QtQuick
import QtQuick.Layouts
import TokenMonitor

ColumnLayout {
    id: trends
    spacing: 10

    function tr(key, fallback) {
        var s = app.i18n.t(key)
        return (!s || s === key) ? fallback : s
    }

    function iso(d) {
        return d.getFullYear() + "-" + ("0" + (d.getMonth() + 1)).slice(-2) + "-" + ("0" + d.getDate()).slice(-2)
    }

    function shortDate(raw) {
        var m = /^(\d{4})-(\d{2})-(\d{2})/.exec(String(raw || ""))
        if (m) return Number(m[2]) + "/" + Number(m[3])
        return String(raw || "")
    }

    function byDate() {
        var map = {}
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i)
            map[String(days[i].date || "")] = Number(days[i].tokens || 0)
        return map
    }

    function weekBars() {
        var map = byDate()
        var out = []
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        for (var back = 6; back >= 0; --back) {
            var d = new Date(today)
            d.setDate(d.getDate() - back)
            var key = iso(d)
            out.push({ date: key, tokens: map[key] || 0 })
        }
        return out
    }

    function maxBar(bars) {
        var m = 1
        for (var i = 0; i < bars.length; ++i) m = Math.max(m, Number(bars[i].tokens || 0))
        return m
    }

    function activeDays() {
        var n = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i)
            if (Number(days[i].tokens || 0) > 0) n++
        return n
    }

    function streak() {
        var map = byDate()
        var n = 0
        var d = new Date()
        d.setHours(0, 0, 0, 0)
        while (map[iso(d)] > 0) {
            n++
            d.setDate(d.getDate() - 1)
        }
        return n
    }

    function selectedKeys() {
        var today = iso(new Date())
        today = today.slice(0, 10)
        if (app.period === "allTime") return null
        if (app.period === "month" || app.period === "week" || app.period === "last30")
            return { month: today.slice(0, 7) }
        return { day: today }
    }

    function activeTimeMs() {
        var sel = selectedKeys()
        var ms = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i) {
            var key = String(days[i].date || "").slice(0, 10)
            if (sel && sel.day && key !== sel.day) continue
            if (sel && sel.month && key.slice(0, 7) !== sel.month) continue
            ms += Number(days[i].activeTimeMs || 0)
        }
        return ms
    }

    function peakTokens() {
        var sel = selectedKeys()
        var peak = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i) {
            var key = String(days[i].date || "").slice(0, 10)
            if (sel && sel.day && key !== sel.day) continue
            if (sel && sel.month && key.slice(0, 7) !== sel.month) continue
            peak = Math.max(peak, Number(days[i].tokens || 0))
        }
        return peak
    }

    function formatDur(ms) {
        var totalMinutes = Math.max(0, Math.round(Number(ms || 0) / 60000))
        var hours = Math.floor(totalMinutes / 60)
        var minutes = totalMinutes % 60
        if (hours > 0) return hours + "h " + minutes + "m"
        if (minutes > 0) return minutes + "m"
        return "0m"
    }

    function rangeLabel() {
        if (app.period === "allTime") return trends.tr("trends.range.year", "Past year")
        if (app.period === "month" || app.period === "week" || app.period === "last30")
            return trends.tr("trends.range.month", "This month")
        return trends.tr("trends.range.week", "Last 7 days")
    }

    RowLayout {
        Layout.fillWidth: true
        Text {
            text: trends.rangeLabel()
            color: Theme.muted
            font.pixelSize: 11
            font.family: Theme.fontFamily
            font.letterSpacing: 0.4
            font.capitalization: Font.AllUppercase
            opacity: 0.6
        }
        Item { Layout.fillWidth: true }
        Text {
            text: "↗"
            color: Theme.muted
            font.pixelSize: 13
            opacity: 0.75
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.PointingHandCursor
                onClicked: app.showDashboard()
            }
        }
    }

    Item {
        id: spark
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        property var bars: trends.weekBars()
        property real peak: trends.maxBar(bars)

        Row {
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: Math.max(2, width * 0.04)
            Repeater {
                model: spark.bars
                Item {
                    width: (spark.width - 8 - (spark.bars.length - 1) * parent.spacing) / Math.max(1, spark.bars.length)
                    height: parent.height
                    Rectangle {
                        visible: Number(modelData.tokens || 0) > 0
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(6, parent.width * 0.72)
                        height: Math.max(2, parent.height * (Number(modelData.tokens || 0) / spark.peak))
                        radius: 2
                        color: "#6ab4f0"
                        opacity: index === spark.bars.length - 1 ? 1 : 0.5
                    }
                    Rectangle {
                        visible: Number(modelData.tokens || 0) <= 0
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(6, parent.width * 0.72)
                        height: 1
                        color: "#6ab4f0"
                        opacity: index === spark.bars.length - 1 ? 0.32 : 0.24
                    }
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: app.showDashboard()
        }
        Connections {
            target: app
            function onStatsChanged() {
                spark.bars = trends.weekBars()
                spark.peak = trends.maxBar(spark.bars)
            }
            function onPeriodChanged() {
                spark.bars = trends.weekBars()
                spark.peak = trends.maxBar(spark.bars)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Text {
            text: spark.bars.length ? trends.shortDate(spark.bars[0].date) : ""
            color: Theme.muted
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
        Item { Layout.fillWidth: true }
        Text {
            text: spark.bars.length ? trends.shortDate(spark.bars[spark.bars.length - 1].date) : ""
            color: Theme.muted
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 8
        rowSpacing: 8

        Repeater {
            model: 4
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.04)
                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        color: Theme.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        text: {
                            var _ = app.historyDays
                            var __ = app.period
                            if (index === 0) return String(trends.activeDays())
                            if (index === 1) return String(trends.streak())
                            if (index === 2) return trends.formatDur(trends.activeTimeMs())
                            return app.formatTokens(trends.peakTokens())
                        }
                    }
                    Text {
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        opacity: 0.55
                        text: {
                            var _ = app.i18n.resolvedLanguage
                            if (index === 0) return trends.tr("trends.activeDays", "Active days")
                            if (index === 1) return trends.tr("trends.currentStreak", "Current streak")
                            if (index === 2) return trends.tr("trends.activeTime", "Active time")
                            return trends.tr("trends.peakDay", "Peak day")
                        }
                    }
                }
            }
        }
    }
}
