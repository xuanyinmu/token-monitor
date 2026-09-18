import QtQuick
import TokenMonitor

Item {
    id: row
    property string icon: ""
    property color markColor: Theme.blue
    property string label: ""
    property string sub: ""
    property string value: ""
    property string aux: ""
    property bool showAux: aux.length > 0
    height: 16
    width: parent ? parent.width : 300

    // Electron .home-model-row / .home-tool-row:
    // 10px mark | name | tokens (minmax 42px) | share (minmax 31px), 8px columns.
    Row {
        anchors.fill: parent
        spacing: 8

        Item {
            width: 10
            height: parent.height
            TintIcon {
                visible: row.icon.length > 0 && row.icon.indexOf("token-monitor") < 0
                anchors.centerIn: parent
                source: row.icon
                size: 10
                tint: Theme.text
            }
            Text {
                visible: row.icon.indexOf("token-monitor") >= 0
                anchors.centerIn: parent
                text: "Σ"
                color: Theme.text
                font.pixelSize: 11
                font.weight: Font.Bold
                font.family: Theme.fontFamily
            }
            Rectangle {
                visible: row.icon.length === 0
                width: 6
                height: 6
                radius: 3
                anchors.centerIn: parent
                color: row.markColor
            }
        }

        Text {
            width: Math.max(40, parent.width - 10 - 8 - valueText.width - (row.showAux ? auxText.width + 8 : 0) - 8)
            anchors.verticalCenter: parent.verticalCenter
            text: row.label
            color: Theme.text
            font.pixelSize: 11
            font.family: Theme.fontFamily
            elide: Text.ElideRight
        }

        Text {
            id: valueText
            anchors.verticalCenter: parent.verticalCenter
            text: row.value
            color: Theme.text
            font.pixelSize: 11
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignRight
        }

        Text {
            id: auxText
            visible: row.showAux
            width: visible ? Math.max(31, implicitWidth) : 0
            anchors.verticalCenter: parent.verticalCenter
            text: row.aux
            color: Theme.muted
            font.pixelSize: 10
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignRight
        }
    }
}
