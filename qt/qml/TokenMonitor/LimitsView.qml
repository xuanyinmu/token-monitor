import QtQuick
import TokenMonitor

Flickable {
    id: limits
    clip: true
    contentWidth: width
    contentHeight: col.height
    boundsBehavior: Flickable.StopAtBounds
    rightMargin: 2

    function remainFrac(v) {
        var n = Number(v)
        if (!isFinite(n)) return 0
        if (Math.abs(n) > 1) return Math.max(0, Math.min(1, n / 100))
        return Math.max(0, Math.min(1, n))
    }

    function rightText(row) {
        var status = String(row.status || "")
        if (status === "notConfigured") return row.statusLabel || "Not signed in"
        if (status === "unauthorized") return row.statusLabel || "Sign in again"
        if (status === "disabled") return row.statusLabel || "Disabled"
        if (status === "unavailable") return row.statusLabel || "Unavailable"
        return row.plan || ""
    }

    Column {
        id: col
        width: parent.width
        spacing: 12

        Repeater {
            model: app.limitRows
            delegate: Column {
                id: provider
                property var row: modelData
                width: col.width
                spacing: 10
                bottomPadding: 13

                Row {
                    width: parent.width
                    spacing: 8
                    TintIcon {
                        visible: (provider.row.icon || "").length > 0
                        source: provider.row.icon || ""
                        size: 12
                        tint: Theme.text
                        anchors.top: parent.top
                        anchors.topMargin: 1
                    }
                    Column {
                        width: parent.width - 20 - rightMeta.width - 16
                        spacing: 2
                        Text {
                            text: provider.row.label
                            color: Theme.text
                            font.pixelSize: 12
                            font.weight: Font.Normal
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                            width: parent.width
                        }
                        Text {
                            visible: String(provider.row.status || "") !== "notConfigured"
                                     && (provider.row.updatedText || "").length > 0
                            text: provider.row.updatedText || ""
                            color: Theme.muted
                            font.pixelSize: 10
                            font.family: Theme.fontFamily
                        }
                    }
                    Text {
                        id: rightMeta
                        text: limits.rightText(provider.row)
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        anchors.top: parent.top
                        anchors.topMargin: 1
                    }
                }

                Column {
                    width: parent.width
                    spacing: 8
                    Repeater {
                        model: provider.row.windows || []
                        Column {
                            width: parent.width
                            spacing: 5
                            Row {
                                width: parent.width
                                Text {
                                    text: modelData.label || modelData.kind || ""
                                    color: Theme.muted
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                    elide: Text.ElideRight
                                    width: parent.width - val.width - 8
                                }
                                Text {
                                    id: val
                                    text: modelData.value || ""
                                    color: Theme.text
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                }
                            }
                            Rectangle {
                                visible: modelData.showMeter !== false
                                width: parent.width
                                height: 6
                                radius: 3
                                color: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.44)
                                Rectangle {
                                    width: parent.width * limits.remainFrac(modelData.remainingPercent)
                                    height: parent.height
                                    radius: 3
                                    color: provider.row.color || Theme.blue
                                }
                            }
                            Text {
                                visible: (modelData.resetText || "").length > 0
                                text: modelData.resetText || ""
                                color: Theme.muted
                                font.pixelSize: 10
                                font.family: Theme.fontFamily
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.12)
                }
            }
        }
    }
}
