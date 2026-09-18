import QtQuick
import TokenMonitor

Item {
    id: root
    property url source
    property int size: 10
    property color tint: Theme.text
    width: size
    height: size

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
        sourceSize.width: Math.max(24, root.size * 3)
        sourceSize.height: Math.max(24, root.size * 3)
        source: {
            var raw = String(root.source || "")
            if (!raw.length) return ""
            var path = raw.replace(/^qrc:\//, "")
            return "image://mask/" + path + "?c=" + encodeURIComponent(String(root.tint))
        }
    }
}
