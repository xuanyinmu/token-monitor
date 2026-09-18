import QtQuick
import TokenMonitor

ListView {
    clip: true
    spacing: 0
    model: app.sessionModel
    boundsBehavior: Flickable.StopAtBounds
    delegate: UsageRow {
        width: ListView.view.width
        label: model.label || ""
        icon: model.icon || ""
        value: app.formatNumber(model.tokens)
        sub: model.extra || ""
        activity: model.activity || ""
        detail: model.detail || ""
        cost: app.formatUsd(model.cost)
        percent: model.percent
        barColor: model.color || Theme.blue
        hoverScroll: true
        barHeight: 5
        MouseArea {
            anchors.fill: parent
            onClicked: app.openSession(model.id)
        }
    }
}
