import QtQuick
import TokenMonitor

Column {
    id: panel
    spacing: 0
    width: parent ? parent.width : 312

    Item { width: 1; height: 2 }

    Text {
        text: "TOTAL TOKENS"
        color: Theme.muted
        font.pixelSize: 11
        font.family: Theme.fontFamily
        leftPadding: 2
    }

    Item { width: 1; height: 8 }

    Row {
        spacing: 8
        leftPadding: 2
        Text {
            id: total
            text: app.totalText
            color: Theme.number
            font.family: Theme.displayFont
            font.weight: Font.Medium
            font.pixelSize: Math.round(Math.max(30, Math.min(46, (panel.width + 28) * 0.11)))
            font.letterSpacing: 0
        }
        Text {
            visible: !!app.settings.showCompactTotalTokens
            anchors.baseline: total.baseline
            text: "≈ " + app.compactText
            color: Theme.muted
            font.pixelSize: 13
            font.weight: Font.Medium
            font.family: Theme.fontFamily
        }
    }

    Item { width: 1; height: 6 }

    Text {
        text: app.costText
        color: Theme.muted
        font.pixelSize: 12
        font.family: Theme.fontFamily
        leftPadding: 2
    }

    Item { width: 1; height: 12 }
}
