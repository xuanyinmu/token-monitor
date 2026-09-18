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
        spacing: 10

        Repeater {
            model: app.serviceStatusRows
            delegate: Item {
                width: col.width
                implicitHeight: body.implicitHeight + 12

                Column {
                    id: body
                    width: parent.width
                    spacing: 5

                    Row {
                        width: parent.width
                        spacing: 8

                        Row {
                            width: parent.width - pill.width - 8
                            spacing: 6
                            TintIcon {
                                visible: (modelData.icon || "") !== ""
                                source: modelData.icon || ""
                                size: 14
                                tint: Theme.text
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.label || ""
                                color: Theme.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                                width: parent.width - 20
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Rectangle {
                            id: pill
                            width: pillText.implicitWidth + 12
                            height: 16
                            radius: 6
                            color: "transparent"
                            border.width: 1
                            border.color: {
                                var s = modelData.status || ""
                                if (s === "ok") return Qt.rgba(183 / 255, 234 / 255, 212 / 255, 0.28)
                                if (s === "degraded") return Qt.rgba(241 / 255, 217 / 255, 115 / 255, 0.32)
                                if (s === "outage") return Qt.rgba(244 / 255, 119 / 255, 136 / 255, 0.42)
                                return Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.14)
                            }
                            Text {
                                id: pillText
                                anchors.centerIn: parent
                                text: modelData.pill || ""
                                color: {
                                    var s = modelData.status || ""
                                    if (s === "ok") return Theme.accent
                                    if (s === "degraded") return Theme.yellow
                                    if (s === "outage") return Theme.red
                                    return Theme.muted
                                }
                                font.pixelSize: 9
                                font.family: Theme.fontFamily
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: modelData.description || ""
                        color: Theme.text
                        font.pixelSize: 11
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: (modelData.meta || "").length > 0
                        width: parent.width
                        text: modelData.meta || ""
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.12)
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (modelData.pageUrl)
                            Qt.openUrlExternally(modelData.pageUrl)
                    }
                }
            }
        }
    }
}
