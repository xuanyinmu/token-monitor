import QtQuick
import QtQml
import TokenMonitor

Item {
    id: heat
    property var days: []
    property string metric: ""
    property bool twelveMonth: false
    property int cell: 9
    property int gap: 3
    readonly property int pitch: cell + gap
    readonly property int canvasWidth: {
        var pitch = cell + gap
        if (!twelveMonth) return 53 * pitch
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        var start = new Date(today.getFullYear(), today.getMonth() - 11, 1)
        start.setDate(start.getDate() - start.getDay())
        var weeks = Math.ceil(((today - start) / 86400000 + 1) / 7)
        return Math.max(1, weeks) * pitch
    }

    function gridStart() {
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        var start = new Date(today)
        if (heat.twelveMonth) {
            start = new Date(today.getFullYear(), today.getMonth() - 11, 1)
            start.setDate(start.getDate() - start.getDay())
        } else {
            start.setDate(start.getDate() - 52 * 7 - start.getDay())
        }
        return start
    }

    function dayAt(mx, my) {
        var row = Math.floor(my / heat.pitch)
        var col = Math.floor(mx / heat.pitch)
        if (row < 0 || row > 6 || col < 0) return null
        var d = heat.gridStart()
        d.setDate(d.getDate() + col * 7 + row)
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        if (d > today) return null
        var iso = d.getFullYear() + "-" + ("0" + (d.getMonth() + 1)).slice(-2) + "-" + ("0" + d.getDate()).slice(-2)
        var val = 0
        var list = heat.days || []
        for (var i = 0; i < list.length; ++i) {
            var key = String(list[i].date || list[i].day || "")
            if (key === iso) val = Number(list[i].tokens || 0)
        }
        return { date: iso, tokens: val, x: col * heat.pitch, y: row * heat.pitch }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        clip: true
        contentWidth: canvas.width
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        Canvas {
            id: canvas
            width: heat.canvasWidth
            height: scroller.height
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var byDate = {}
                var max = 0
                var list = heat.days || []
                var metricCost = heat.metric === "tokens" ? false
                                  : heat.metric === "cost" ? true
                                  : !(app && app.settings && app.settings.heatmapMetric === "tokens")
                for (var i = 0; i < list.length; ++i) {
                    var rec = list[i]
                    var key = String(rec.date || rec.day || "")
                    var v = Number(metricCost ? (rec.cost || 0) : (rec.tokens || 0))
                    byDate[key] = v
                    if (v > max) max = v
                }
                var cell = heat.cell
                var gap = heat.gap
                var today = new Date()
                today.setHours(0, 0, 0, 0)
                var start = new Date(today)
                if (heat.twelveMonth) {
                    start = new Date(today.getFullYear(), today.getMonth() - 11, 1)
                    start.setDate(start.getDate() - start.getDay())
                } else {
                    start.setDate(start.getDate() - 52 * 7 - start.getDay())
                }
                var levels = [
                    Qt.rgba(1, 1, 1, 0.03),
                    Qt.rgba(90 / 255, 170 / 255, 1, 0.18),
                    Qt.rgba(120 / 255, 190 / 255, 1, 0.45),
                    Qt.rgba(150 / 255, 210 / 255, 1, 0.8),
                    Qt.rgba(180 / 255, 230 / 255, 1, 1)
                ]
                function roundRect(ctx, x, y, w, h, r) {
                    ctx.beginPath()
                    ctx.moveTo(x + r, y)
                    ctx.arcTo(x + w, y, x + w, y + h, r)
                    ctx.arcTo(x + w, y + h, x, y + h, r)
                    ctx.arcTo(x, y + h, x, y, r)
                    ctx.arcTo(x, y, x + w, y, r)
                    ctx.closePath()
                    ctx.fill()
                }
                var lang = "en"
                if (app && app.i18n && app.i18n.resolvedLanguage)
                    lang = String(app.i18n.resolvedLanguage).replace("-", "_")
                var loc = Qt.locale(lang)
                var d = new Date(start)
                var lastMonth = -1
                var labelY = 7 * (cell + gap) - gap + 12
                ctx.font = "9px " + Theme.fontFamily
                while (d <= today) {
                    var row = d.getDay()
                    var col = Math.floor((d - start) / 86400000 / 7)
                    var iso = d.getFullYear() + "-" + ("0" + (d.getMonth() + 1)).slice(-2) + "-" + ("0" + d.getDate()).slice(-2)
                    var val = byDate[iso] || 0
                    var lvl = 0
                    if (max > 0 && val > 0) {
                        var ratio = val / max
                        lvl = ratio >= 0.75 ? 4 : ratio >= 0.5 ? 3 : ratio >= 0.25 ? 2 : 1
                    }
                    var x = col * (cell + gap)
                    var y = row * (cell + gap)
                    ctx.fillStyle = levels[lvl]
                    roundRect(ctx, x, y, cell, cell, 2)
                    if (d.getDate() === 1 && d.getMonth() !== lastMonth) {
                        lastMonth = d.getMonth()
                        ctx.fillStyle = Theme.muted
                        ctx.fillText(loc.standaloneMonthName(d.getMonth(), Locale.ShortFormat), x, labelY)
                    }
                    d.setDate(d.getDate() + 1)
                }
            }
            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                onPositionChanged: function(mouse) {
                    var hit = heat.dayAt(mouse.x, mouse.y)
                    if (!hit) {
                        tip.visible = false
                        return
                    }
                    tipTokens.text = (app && app.formatTokens ? app.formatTokens(hit.tokens) : String(hit.tokens)) + " tokens"
                    tipDate.text = hit.date
                    tip.visible = true
                    var localX = hit.x - scroller.contentX + 12
                    tip.x = Math.min(heat.width - tip.width - 4, Math.max(4, localX))
                    tip.y = Math.max(2, hit.y - tip.height - 6)
                }
                onExited: tip.visible = false
            }
        }
    }

    Rectangle {
        id: tip
        visible: false
        z: 20
        width: tipCol.implicitWidth + 16
        height: tipCol.implicitHeight + 12
        radius: 6
        color: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.94)
        border.width: 1
        border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.16)
        Column {
            id: tipCol
            x: 8
            y: 6
            spacing: 2
            Text {
                id: tipTokens
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                color: Theme.text
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.family: Theme.fontFamily
            }
            Text {
                id: tipDate
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
            }
        }
    }

    function scrollToEnd() {
        scroller.contentX = Math.max(0, scroller.contentWidth - scroller.width)
    }

    onMetricChanged: canvas.requestPaint()
    onTwelveMonthChanged: {
        canvas.requestPaint()
        Qt.callLater(heat.scrollToEnd)
    }
    Connections {
        target: heat
        function onDaysChanged() {
            canvas.requestPaint()
            Qt.callLater(heat.scrollToEnd)
        }
    }
    Connections {
        target: app && app.i18n ? app.i18n : null
        function onLanguageChanged() { canvas.requestPaint() }
    }
    Component.onCompleted: Qt.callLater(heat.scrollToEnd)
    onWidthChanged: Qt.callLater(heat.scrollToEnd)
}
