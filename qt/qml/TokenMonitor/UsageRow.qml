import QtQuick
import TokenMonitor

Item {
    id: row
    property string mark: ""
    property string icon: ""
    property string label: ""
    property string value: ""
    property string sub: ""
    property string activity: ""
    property string cost: ""
    property string detail: ""
    property real percent: 0
    property bool local: false
    property bool hoverScroll: false
    property color barColor: Theme.blue
    property int barHeight: 6
    implicitHeight: col.implicitHeight + (row.hoverScroll ? 8 : 10)
    width: parent ? parent.width : 300

    Column {
        id: col
        width: parent.width
        spacing: row.hoverScroll ? 5 : 6

        Row {
            width: parent.width
            spacing: 10

            Row {
                width: parent.width - metrics.width - 10
                spacing: 8
                TintIcon {
                    visible: row.icon.length > 0
                    anchors.verticalCenter: parent.verticalCenter
                    source: row.icon
                    size: 10
                    tint: Theme.text
                }
                Rectangle {
                    visible: row.icon.length === 0
                    width: 8
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.blue
                }
                Column {
                    width: parent.width - 18
                    spacing: 2
                    Row {
                        width: parent.width
                        spacing: 6
                        Item {
                            id: labelClip
                            width: parent.width - (row.local ? 28 : 0)
                            height: lab.implicitHeight
                            clip: true
                            Text {
                                id: lab
                                width: row.hoverScroll ? implicitWidth : parent.width
                                text: row.label
                                color: Theme.text
                                font.pixelSize: 12
                                font.family: Theme.fontFamily
                                elide: row.hoverScroll ? Text.ElideNone : Text.ElideRight
                            }
                            NumberAnimation {
                                id: labelMarquee
                                target: lab
                                property: "x"
                                from: 0
                                to: Math.min(0, labelClip.width - lab.implicitWidth)
                                duration: Math.max(1800, Math.min(8000, Math.ceil(Math.max(0, lab.implicitWidth - labelClip.width) * 22)))
                                easing.type: Easing.Linear
                            }
                            Timer {
                                id: labelDelay
                                interval: 240
                                repeat: false
                                onTriggered: if (row.hoverScroll && lab.implicitWidth > labelClip.width + 1) labelMarquee.start()
                            }
                            HoverHandler {
                                enabled: row.hoverScroll
                                onHoveredChanged: {
                                    if (hovered) {
                                        labelDelay.restart()
                                    } else {
                                        labelDelay.stop()
                                        labelMarquee.stop()
                                        lab.x = 0
                                    }
                                }
                            }
                        }
                        Rectangle {
                            visible: row.local
                            width: youLab.implicitWidth + 10
                            height: 14
                            radius: 5
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.13)
                            Text {
                                id: youLab
                                anchors.centerIn: parent
                                text: "you"
                                color: Theme.muted
                                font.pixelSize: 9
                                font.family: Theme.fontFamily
                            }
                        }
                    }
                    Text {
                        visible: row.sub.length > 0
                        width: parent.width
                        text: row.sub
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: row.activity.length > 0
                        width: parent.width
                        text: row.activity
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }
                    Item {
                        id: detailClip
                        visible: row.detail.length > 0
                        width: parent.width
                        height: det.implicitHeight
                        clip: true
                        Text {
                            id: det
                            width: row.hoverScroll ? implicitWidth : parent.width
                            text: row.detail
                            color: Theme.muted
                            font.pixelSize: 10
                            font.family: Theme.fontFamily
                            elide: row.hoverScroll ? Text.ElideNone : Text.ElideRight
                            opacity: 0.84
                        }
                        NumberAnimation {
                            id: detailMarquee
                            target: det
                            property: "x"
                            from: 0
                            to: Math.min(0, detailClip.width - det.implicitWidth)
                            duration: Math.max(1800, Math.min(8000, Math.ceil(Math.max(0, det.implicitWidth - detailClip.width) * 22)))
                            easing.type: Easing.Linear
                        }
                        Timer {
                            id: detailDelay
                            interval: 240
                            repeat: false
                            onTriggered: if (row.hoverScroll && det.implicitWidth > detailClip.width + 1) detailMarquee.start()
                        }
                        HoverHandler {
                            enabled: row.hoverScroll
                            onHoveredChanged: {
                                if (hovered) {
                                    detailDelay.restart()
                                } else {
                                    detailDelay.stop()
                                    detailMarquee.stop()
                                    det.x = 0
                                }
                            }
                        }
                    }
                }
            }

            Column {
                id: metrics
                Text {
                    text: row.value
                    color: Theme.text
                    font.pixelSize: 12
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignRight
                    anchors.right: parent.right
                }
                Text {
                    visible: row.cost.length > 0
                    text: row.cost
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignRight
                    anchors.right: parent.right
                }
            }
        }

        Rectangle {
            width: parent.width
            height: row.barHeight
            radius: 3
            color: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.46)
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, row.percent))
                height: parent.height
                radius: 3
                color: row.barColor
                Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.12)
    }
}
