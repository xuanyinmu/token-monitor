import QtQuick
import TokenMonitor

Item {
    id: chart
    property var points: []

    function shortDate(raw) {
        var s = String(raw || "")
        var m = /^(\d{4})-(\d{2})-(\d{2})/.exec(s)
        if (m) return Number(m[2]) + "/" + Number(m[3])
        return s.slice(5)
    }

    function dateAt(idx) {
        if (!chart.points || !chart.points.length) return ""
        var start = Math.max(0, chart.points.length - 45)
        var i = Math.max(start, Math.min(chart.points.length - 1, idx))
        return shortDate(chart.points[i].date || "")
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.bottomMargin: 12
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var pts = chart.points
            if (!pts || pts.length < 2) return
            var max = 0
            var start = Math.max(0, pts.length - 45)
            var slice = []
            for (var i = start; i < pts.length; ++i) {
                slice.push(pts[i])
                max = Math.max(max, Number(pts[i].value || 0))
            }
            if (slice.length < 2) return
            var padT = 4, padR = 3, padB = 4, padL = 3
            var innerW = width - padL - padR
            var innerH = height - padT - padB
            var xOf = function (idx) { return padL + innerW * idx / Math.max(1, slice.length - 1) }
            var yOf = function (v) { return padT + innerH - (max > 0 ? v / max : 0) * innerH }
            var xy = []
            for (var j = 0; j < slice.length; ++j)
                xy.push({ x: xOf(j), y: yOf(Number(slice[j].value || 0)) })
            ctx.beginPath()
            ctx.moveTo(xy[0].x, xy[0].y)
            if (xy.length < 3) {
                ctx.lineTo(xy[1].x, xy[1].y)
            } else {
                for (var k = 0; k < xy.length - 1; ++k) {
                    var p0 = xy[Math.max(0, k - 1)]
                    var p1 = xy[k]
                    var p2 = xy[k + 1]
                    var p3 = xy[Math.min(xy.length - 1, k + 2)]
                    ctx.bezierCurveTo(
                        p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6,
                        p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6,
                        p2.x, p2.y)
                }
            }
            ctx.lineTo(xy[xy.length - 1].x, padT + innerH)
            ctx.lineTo(xy[0].x, padT + innerH)
            ctx.closePath()
            var grad = ctx.createLinearGradient(0, padT, 0, padT + innerH)
            grad.addColorStop(0, Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.22))
            grad.addColorStop(1, Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0))
            ctx.fillStyle = grad
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(xy[0].x, xy[0].y)
            if (xy.length < 3) {
                ctx.lineTo(xy[1].x, xy[1].y)
            } else {
                for (var n = 0; n < xy.length - 1; ++n) {
                    var a0 = xy[Math.max(0, n - 1)]
                    var a1 = xy[n]
                    var a2 = xy[n + 1]
                    var a3 = xy[Math.min(xy.length - 1, n + 2)]
                    ctx.bezierCurveTo(
                        a1.x + (a2.x - a0.x) / 6, a1.y + (a2.y - a0.y) / 6,
                        a2.x - (a3.x - a1.x) / 6, a2.y - (a3.y - a1.y) / 6,
                        a2.x, a2.y)
                }
            }
            ctx.strokeStyle = Theme.blue
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.stroke()
        }
        Connections {
            target: chart
            function onPointsChanged() { canvas.requestPaint() }
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Row {
        anchors.bottom: parent.bottom
        width: parent.width
        Text {
            width: parent.width / 3
            text: chart.dateAt(Math.max(0, (chart.points ? chart.points.length : 0) - 45))
            color: Theme.muted
            font.pixelSize: 9
            font.family: Theme.fontFamily
        }
        Text {
            width: parent.width / 3
            horizontalAlignment: Text.AlignHCenter
            text: {
                if (!chart.points || !chart.points.length) return ""
                var start = Math.max(0, chart.points.length - 45)
                return chart.dateAt(Math.floor((start + chart.points.length - 1) / 2))
            }
            color: Theme.muted
            font.pixelSize: 9
            font.family: Theme.fontFamily
        }
        Text {
            width: parent.width / 3
            horizontalAlignment: Text.AlignRight
            text: chart.points && chart.points.length ? chart.dateAt(chart.points.length - 1) : ""
            color: Theme.muted
            font.pixelSize: 9
            font.family: Theme.fontFamily
        }
    }
}
