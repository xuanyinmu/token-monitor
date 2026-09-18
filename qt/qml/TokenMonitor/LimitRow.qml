import QtQuick
import TokenMonitor

Item {
    id: row
    property string label: ""
    property string detail: ""
    property string status: "ok"
    property string icon: ""
    property real percent: 0
    property bool remainingMeter: false
    readonly property bool critical: remainingMeter ? percent < 0.15 : percent > 0.85
    readonly property bool warning: remainingMeter ? percent < 0.4 : percent > 0.6
    implicitHeight: col.implicitHeight
    width: parent ? parent.width : 300

    Column {
        id: col
        width: parent.width
        spacing: 4

        Row {
            spacing: 8
            TintIcon {
                visible: row.icon.length > 0
                source: row.icon
                size: 12
                tint: Theme.text
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: row.label
                color: Theme.text
                font.pixelSize: 12
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                width: Math.max(40, row.width - 80)
            }
        }

        Row {
            width: parent.width
            spacing: 8
            Text {
                text: row.detail
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                width: parent.width - pct.width - 8
            }
            Text {
                id: pct
                visible: row.detail.length > 0 || row.percent > 0
                text: row.detail.length ? row.detail : (Math.round(row.percent * 100) + "%")
                color: row.critical ? Theme.red : (row.warning ? Theme.orange : Theme.text)
                font.pixelSize: 10
                font.family: Theme.fontFamily
            }
        }

        Rectangle {
            width: parent.width
            height: 6
            radius: 3
            color: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.46)
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, row.percent))
                height: parent.height
                radius: 3
                color: row.critical ? Theme.red : (row.warning ? Theme.orange : Theme.blue)
            }
        }
    }
}
