import QtQuick
import TokenMonitor

ListView {
    clip: true
    spacing: 0
    model: app.projectModel
    boundsBehavior: Flickable.StopAtBounds
    delegate: UsageRow {
        width: ListView.view.width
        label: model.label
        value: app.formatNumber(model.tokens)
        cost: app.formatUsd(model.cost)
        percent: model.percent
        barColor: Theme.blue
    }
}
