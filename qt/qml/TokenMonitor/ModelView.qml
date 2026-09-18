import QtQuick
import TokenMonitor

Flickable {
    clip: true
    contentWidth: width
    contentHeight: col.height
    boundsBehavior: Flickable.StopAtBounds
    Column {
        id: col
        width: parent.width
        Repeater {
            model: app.modelRows
            UsageRow {
                width: col.width
                label: modelData.label
                icon: modelData.icon || ""
                value: app.formatNumber(modelData.tokens)
                cost: app.formatUsd(modelData.cost)
                percent: modelData.percent
                barColor: modelData.color || Theme.blue
            }
        }
    }
}
