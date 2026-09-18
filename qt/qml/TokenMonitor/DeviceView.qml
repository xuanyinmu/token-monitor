import QtQuick
import TokenMonitor

ListView {
    clip: true
    spacing: 0
    model: app.deviceModel
    boundsBehavior: Flickable.StopAtBounds
    delegate: UsageRow {
        width: ListView.view.width
        label: model.label || model.id
        value: app.formatNumber(model.tokens)
        cost: app.formatUsd(model.cost)
        sub: model.extra || model.platform || ""
        percent: model.percent
        local: model.local === true
        barColor: model.stale ? Theme.muted : Theme.blue
    }
}
