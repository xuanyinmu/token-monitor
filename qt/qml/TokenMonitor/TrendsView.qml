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
        var mo = /^(\d{4})-(\d{2})$/.exec(String(raw || ""))
        if (mo) return Number(mo[2]) + "月"
        return String(raw || "")
    }

    function byDate() {
        var map = {}
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i)
            map[String(days[i].date || "")] = Number(days[i].tokens || 0)
        return map
    }

    function dailyBars(startKey, endKey) {
        var map = byDate()
        var out = []
        // History omits zero days; the selected range shows every calendar
        // day in it, zero-filled, with today already live-patched upstream.
        var cursor = new Date(startKey + "T00:00:00")
        var end = new Date(endKey + "T00:00:00")
        while (cursor <= end && out.length < 62) {
            var key = iso(cursor)
            out.push({ date: key, tokens: map[key] || 0 })
            cursor.setDate(cursor.getDate() + 1)
        }
        return out
    }

    function monthlyBars() {
        // Electron's all-time series is the monthly rollup (trends.range.year).
        var days = app.historyDays || []
        var map = {}
        var keys = []
        for (var i = 0; i < days.length; ++i) {
            var key = String(days[i].date || "").slice(0, 7)
            if (!key) continue
            if (map[key] === undefined) {
                map[key] = 0
                keys.push(key)
            }
            map[key] += Number(days[i].tokens || 0)
        }
        keys.sort()
        var out = []
        for (var j = Math.max(0, keys.length - 13); j < keys.length; ++j)
            out.push({ date: keys[j], tokens: map[keys[j]] })
        return out
    }

    // The chart follows the selected period's actual usage (per the user, a
    // deliberate divergence from Electron, which keeps the long-range chart
    // for fixed ranges): fixed ranges and MONTH show their daily bars, TOTAL
    // shows the monthly rollup, DAY keeps the trailing 7-day context.
    function rangeBars() {
        var fixed = fixedRange()
        if (fixed) return dailyBars(fixed.start, fixed.end)
        if (app.period === "allTime") return monthlyBars()
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        var endKey = iso(today)
        if (app.period === "month") {
            var start = new Date(today)
            start.setDate(1)
            return dailyBars(iso(start), endKey)
        }
        var back = new Date(today)
        back.setDate(back.getDate() - 6)
        return dailyBars(iso(back), endKey)
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

    // Fixed ranges (本周/最近 7 天/最近 30 天) scope the active-time and peak
    // cards to the selected date range; native periods scope to the calendar
    // month / today, like Electron activityStatsForPeriod.
    function fixedRange() {
        if (!app.fixedPeriodActive) return null
        var r = app.fixedPeriodRange
        if (!r || r.length < 2) return null
        return { start: String(r[0]), end: String(r[1]) }
    }

    function activeTimeMs() {
        var sel = selectedKeys()
        var fixed = fixedRange()
        var ms = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i) {
            var key = String(days[i].date || "").slice(0, 10)
            if (fixed) {
                if (key < fixed.start || key > fixed.end) continue
            } else if (sel) {
                if (sel.day && key !== sel.day) continue
                if (sel.month && key.slice(0, 7) !== sel.month) continue
            }
            ms += Number(days[i].activeTimeMs || 0)
        }
        return ms
    }

    function peakTokens() {
        var sel = selectedKeys()
        var fixed = fixedRange()
        var peak = 0
        var days = app.historyDays || []
        for (var i = 0; i < days.length; ++i) {
            var key = String(days[i].date || "").slice(0, 10)
            if (fixed) {
                if (key < fixed.start || key > fixed.end) continue
            } else if (sel) {
                if (sel.day && key !== sel.day) continue
                if (sel.month && key.slice(0, 7) !== sel.month) continue
            }
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
        var fixedKey = { week: "periodRange.week", last7: "periodRange.last7", last30: "periodRange.last30" }[app.period]
        if (fixedKey) return trends.tr(fixedKey, { week: "This week", last7: "Last 7 days", last30: "Last 30 days" }[app.period])
        if (app.period === "allTime") return trends.tr("trends.range.year", "Past year")
        if (app.period === "month") return trends.tr("trends.range.month", "This month")
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
        property var bars: trends.rangeBars()
        property real peak: trends.maxBar(bars)
        property int hoverIndex: -1

        // Layout metrics shared by the bar Row and the hover tip so the tip can
        // center on the hovered bar.
        function slotGap(rowWidth) {
            var n = Math.max(1, bars.length)
            var usable = rowWidth - 8
            return Math.max(2, Math.min(rowWidth * 0.04, (usable - 6 * n) / Math.max(1, n - 1)))
        }
        function barWidth(rowWidth) {
            var n = Math.max(1, bars.length)
            return (rowWidth - 8 - (n - 1) * slotGap(rowWidth)) / n
        }

        Row {
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            // 30-bar ranges need tighter gaps than the 7-bar ones; shrink the
            // spacing so every bar keeps at least its 6px minimum width.
            spacing: spark.slotGap(width)
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
                        opacity: index === spark.bars.length - 1 ? 1 : (spark.hoverIndex === index ? 0.85 : 0.5)
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
            hoverEnabled: true
            onClicked: app.showDashboard()
            onPositionChanged: function(mouse) {
                // The tooltip is for the bar itself: the pointer must sit
                // inside the bar's rectangle, not just anywhere in its column.
                spark.hoverIndex = -1
                var gap = spark.slotGap(spark.width)
                var barW = spark.barWidth(spark.width)
                var idx = Math.floor(mouse.x / (barW + gap))
                if (idx < 0 || idx >= spark.bars.length) return
                var tokens = Number(spark.bars[idx].tokens || 0)
                if (tokens <= 0) return
                var visualW = Math.max(6, barW * 0.72)
                var barX = 4 + idx * (barW + gap) + (barW - visualW) / 2
                if (mouse.x < barX || mouse.x > barX + visualW) return
                var rowHeight = spark.height - 12
                var barHeight = Math.max(2, rowHeight * (tokens / spark.peak))
                if (mouse.y >= 6 + (rowHeight - barHeight))
                    spark.hoverIndex = idx
            }
            onExited: spark.hoverIndex = -1
        }
        // Same tooltip treatment as the Home activity heatmap: dark card above
        // the hovered bar with the token count and its date.
        Rectangle {
            id: barTip
            visible: spark.hoverIndex >= 0 && spark.hoverIndex < spark.bars.length
            z: 20
            width: barTipCol.implicitWidth + 16
            height: barTipCol.implicitHeight + 12
            radius: 6
            color: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.94)
            border.width: 1
            border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.16)
            x: {
                var pitch = spark.barWidth(spark.width) + spark.slotGap(spark.width)
                var center = 4 + spark.hoverIndex * pitch + spark.barWidth(spark.width) / 2
                return Math.max(2, Math.min(spark.width - width - 2, center - width / 2))
            }
            y: {
                var rowHeight = spark.height - 12
                var tokens = visible ? Number(spark.bars[spark.hoverIndex].tokens || 0) : 0
                var barHeight = Math.max(2, rowHeight * (tokens / spark.peak))
                return Math.max(2, 6 + (rowHeight - barHeight) - height - 6)
            }
            Column {
                id: barTipCol
                x: 8
                y: 6
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    text: {
                        if (!barTip.visible) return ""
                        var tokens = Number(spark.bars[spark.hoverIndex].tokens || 0)
                        return (app && app.formatTokens ? app.formatTokens(tokens) : String(tokens)) + " tokens"
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    text: barTip.visible && spark.hoverIndex < spark.bars.length
                          ? String(spark.bars[spark.hoverIndex].date || "") : ""
                }
            }
        }
        Connections {
            target: app
            function onStatsChanged() {
                spark.bars = trends.rangeBars()
                spark.peak = trends.maxBar(spark.bars)
            }
            function onPeriodChanged() {
                spark.bars = trends.rangeBars()
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
