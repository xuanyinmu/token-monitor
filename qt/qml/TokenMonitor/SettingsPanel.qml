import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TokenMonitor

Flickable {
    id: panel
    clip: true
    contentWidth: width
    contentHeight: col.height
    boundsBehavior: Flickable.StopAtBounds
    implicitHeight: col.height
    implicitWidth: width > 0 ? width : 312
    onWidthChanged: col.width = width

    function tr(key, fallback) {
        var s = app.i18n.t(key)
        return (!s || s === key) ? fallback : s
    }

    function summaryFor(id) {
        var on = panel.tr("settings.summary.on", "on")
        var off = panel.tr("settings.summary.off", "off")
        if (id === "general") {
            var s = app.i18n.t("settings.summary.general", { startup: app.settings.startAtLogin ? on : off })
            return (!s || s === "settings.summary.general") ? ((app.settings.startAtLogin ? on : off) + " · updates") : s
        }
        if (id === "main") {
            var vis = app.visibleViewCount()
            var tot = app.totalViewCount()
            var s2 = app.i18n.t("settings.summary.views", { visible: vis, total: tot })
            return (!s2 || s2 === "settings.summary.views") ? (vis + "/" + tot + " views") : s2
        }
        if (id === "window") {
            var b = app.settings.windowBehavior || "floating"
            return panel.tr("settings.windowBehavior." + b, "Floating above apps")
        }
        if (id === "appearance") {
            var preset = (app.theme && app.theme.preset) ? app.theme.preset : "default"
            return panel.tr("settings.appearance.preset." + preset, preset)
        }
        if (id === "collection") {
            var c = app.clientHealthCounts()
            if (c && c.healthy !== undefined) {
                var sH = app.i18n.t("settings.summary.toolsHealth", { healthy: c.healthy, review: c.review, unavailable: c.unavailable })
                if (sH && sH !== "settings.summary.toolsHealth") return sH
            }
            var raw = String(app.settings.clients || "")
            var n = raw.split(",").filter(function (x) { return String(x).trim().length > 0 }).length
            var s3 = app.i18n.t("settings.summary.tools", { tracked: n, visible: n, pinned: 0 })
            return (!s3 || s3 === "settings.summary.tools") ? "" : s3
        }
        if (id === "limits") {
            var n = String(app.settings.limitProviders || "").split(",").filter(function (x) { return x.length > 0 }).length
            var s4 = app.i18n.t("settings.summary.limits", { enabled: n || 3, refresh: panel.tr("settings.summary.minutes", "5 min").replace("{minutes}", "5") })
            return (!s4 || s4 === "settings.summary.limits") ? "" : s4
        }
        if (id === "subscriptions") {
            var c = (app.subscriptions || []).length
            if (!c) return panel.tr("settings.subscriptions.summaryEmpty", "None added")
            return String(c)
        }
        if (id === "sync") {
            var mode = app.settings.hubMode || "local"
            if (mode === "host") return panel.tr("settings.sync.hostHub", "Host hub")
            if (mode === "client") return panel.tr("settings.sync.connectHub", "Connect to hub")
            return panel.tr("settings.sync.localOnly", "This device only")
        }
        return ""
    }

    property var open: ({})
    property bool homeOpen: false
    property bool statusOpen: false
    property bool projectOpen: false
    property bool trendOpen: false
    property int summaryTick: 0
    Connections {
        target: app
        function onStatsChanged() { panel.summaryTick++ }
        function onSettingsChanged() { panel.summaryTick++ }
    }

    Column {
        id: col
        width: panel.width
        spacing: 0

        Repeater {
            model: [
                { id: "general", titleKey: "settings.sections.general", title: "General", icon: "settings/general.svg" },
                { id: "main", titleKey: "settings.sections.main", title: "Main", icon: "settings/main.svg" },
                { id: "window", titleKey: "settings.sections.window", title: "Window", icon: "settings/window.svg" },
                { id: "appearance", titleKey: "settings.appearance.title", title: "Appearance", icon: "settings/appearance.svg" },
                { id: "collection", titleKey: "settings.sections.collection", title: "Collection", icon: "settings/collection.svg" },
                { id: "limits", titleKey: "settings.limits.title", title: "AI Tool Limits", icon: "settings/limits.svg" },
                { id: "subscriptions", titleKey: "settings.subscriptions.title", title: "Subscriptions", icon: "settings/subscriptions.svg" },
                { id: "sync", titleKey: "settings.sync.title", title: "Sync", icon: "settings/sync.svg" }
            ]
            delegate: Item {
                id: sectionRoot
                width: panel.width
                implicitHeight: sectionCol.implicitHeight
                height: implicitHeight

                Rectangle {
                    anchors.fill: parent
                    color: open[modelData.id] ? Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.55) : Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.35)
                    border.width: 1
                    border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.10)
                    topLeftRadius: index === 0 ? 12 : 0
                    topRightRadius: index === 0 ? 12 : 0
                    bottomLeftRadius: index === 7 ? 12 : 0
                    bottomRightRadius: index === 7 ? 12 : 0
                    clip: true
                }

                Column {
                    id: sectionCol
                    width: panel.width
                    spacing: 0
                    Rectangle {
                        width: parent.width
                        implicitHeight: 40
                        height: 40
                        color: "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 11
                            anchors.rightMargin: 22
                            spacing: 8
                            TintIcon {
                                Layout.preferredWidth: 15
                                Layout.preferredHeight: 15
                                source: app.uiIcon(modelData.icon)
                                size: 15
                                tint: open[modelData.id] ? Theme.accent : Theme.muted
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: panel.tr(modelData.titleKey, modelData.title)
                                color: Theme.text
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.maximumWidth: 150
                                text: { var _ = panel.summaryTick; return panel.summaryFor(modelData.id) }
                                color: Theme.muted
                                font.pixelSize: 10
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                        Canvas {
                            anchors.right: parent.right
                            anchors.rightMargin: 11
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8
                            height: 8
                            rotation: open[modelData.id] ? 180 : 0
                            Behavior on rotation { NumberAnimation { duration: 250; easing.type: Easing.InOutCubic } }
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = Theme.muted
                                ctx.lineWidth = 1.5
                                ctx.beginPath()
                                ctx.moveTo(1, 3)
                                ctx.lineTo(4, 6)
                                ctx.lineTo(7, 3)
                                ctx.stroke()
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var id = modelData.id
                                var next = {}
                                if (!open[id]) next[id] = true
                                open = next
                            }
                        }
                    }
                    Rectangle {
                        visible: bodyClip.height > 1
                        width: parent.width
                        height: 1
                        color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.08)
                    }
                    Item {
                        id: bodyClip
                        width: parent.width
                        clip: true
                        readonly property bool wantOpen: open[modelData.id] === true
                        height: wantOpen ? bodyLoader.implicitHeight : 0
                        Behavior on height {
                            NumberAnimation {
                                duration: 250
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: [0.4, 0.0, 0.2, 1.0, 1.0, 1.0]
                            }
                        }
                        opacity: height > 8 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        Loader {
                            id: bodyLoader
                            width: parent.width
                            active: true
                            sourceComponent: {
                                switch (modelData.id) {
                                case "general": return generalComp
                                case "main": return mainComp
                                case "window": return windowComp
                                case "appearance": return appearanceComp
                                case "collection": return collectionComp
                                case "limits": return limitsComp
                                case "subscriptions": return subsComp
                                default: return syncComp
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component SettingRow: Column {
        id: srow
        property string label: ""
        property string desc: ""
        property bool hairline: true
        default property alias extra: controlSlot.data
        width: parent ? parent.width : 280
        spacing: 0

        Rectangle {
            visible: srow.hairline
            width: parent.width
            height: 1
            color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.06)
        }
        Item {
            width: parent.width
            height: 32
            Text {
                anchors.left: parent.left
                anchors.right: controlSlot.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: srow.label
                color: Theme.text
                font.pixelSize: 11
                font.family: Theme.fontFamily
                elide: Text.ElideRight
            }
            Item {
                id: controlSlot
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: {
                    if (!children.length) return 0
                    var c = children[0]
                    return Math.max(c.implicitWidth, c.width)
                }
                implicitHeight: {
                    if (!children.length) return 20
                    var c = children[0]
                    return Math.max(c.implicitHeight, c.height, 20)
                }
                width: implicitWidth
                height: implicitHeight
            }
        }
        Text {
            visible: srow.desc.length > 0
            width: parent.width
            topPadding: 0
            bottomPadding: 2
            text: srow.desc
            color: Theme.muted
            font.pixelSize: 10
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    component SubHead: Item {
        property string title: ""
        property string meta: ""
        width: parent ? parent.width : 280
        height: 18
        Text {
            anchors.left: parent.left
            anchors.right: metaText.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: title
            color: Theme.muted
            font.pixelSize: 9
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            opacity: 0.9
            elide: Text.ElideRight
        }
        Text {
            id: metaText
            visible: meta.length > 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: meta
            color: Theme.muted
            font.pixelSize: 9
            font.family: Theme.fontFamily
            opacity: 0.72
        }
    }

    component GlassCombo: ComboBox {
        id: box
        implicitHeight: 28
        Text {
            id: comboProbe
            visible: false
            width: 2000
            wrapMode: Text.NoWrap
            elide: Text.ElideNone
            font.pixelSize: 11
            font.family: Theme.fontFamily
            text: box.displayText
        }
        implicitWidth: Math.min(panel.width * 0.58, Math.max(88, Math.ceil(comboProbe.contentWidth) + 48))
        width: implicitWidth
        palette.window: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
        palette.base: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
        palette.button: Qt.rgba(1, 1, 1, 0.05)
        palette.text: Theme.text
        palette.buttonText: Theme.text
        palette.highlight: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
        palette.highlightedText: Theme.accent
        palette.midlight: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
        palette.light: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
        background: Rectangle {
            implicitHeight: 28
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.05)
            border.color: Theme.line
            border.width: 1
        }
        contentItem: Text {
            leftPadding: 9
            rightPadding: 16
            text: box.displayText
            color: Theme.text
            font.pixelSize: 11
            font.family: Theme.fontFamily
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Canvas {
            x: box.width - 16
            y: (box.height - 8) / 2
            width: 8
            height: 8
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = Theme.muted
                ctx.lineWidth = 1.4
                ctx.beginPath()
                ctx.moveTo(1, 2.5)
                ctx.lineTo(4, 5.5)
                ctx.lineTo(7, 2.5)
                ctx.stroke()
            }
        }
        delegate: ItemDelegate {
            id: comboDel
            width: box.popup.width
            implicitHeight: 28
            highlighted: box.highlightedIndex === index
            hoverEnabled: true
            palette.highlight: "transparent"
            palette.highlightedText: Theme.accent
            contentItem: Text {
                text: modelData
                color: comboDel.highlighted ? Theme.accent : Theme.text
                font.pixelSize: 11
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }
            background: Rectangle {
                radius: 5
                color: comboDel.highlighted
                       ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                       : (comboDel.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
            }
        }
        popup: Popup {
            popupType: Popup.Item
            y: box.height + 4
            width: box.width
            padding: 6
            closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
            palette.window: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
            palette.base: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
            palette.highlight: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
            palette.highlightedText: Theme.accent
            background: Rectangle {
                radius: 8
                color: Qt.rgba(16 / 255, 21 / 255, 30 / 255, 0.96)
                border.width: 1
                border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.16)
            }
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: box.popup.visible ? box.delegateModel : null
                currentIndex: box.highlightedIndex
                spacing: 1
                highlight: null
                highlightFollowsCurrentItem: false
            }
        }
    }

    component GlassButton: Button {
        implicitHeight: 30
        implicitWidth: Math.max(88, contentItem.implicitWidth + 20)
        background: Rectangle {
            radius: 8
            color: parent.down ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.05)
            border.color: Theme.line
            border.width: 1
        }
        contentItem: Text {
            text: parent.text
            color: Theme.text
            font.pixelSize: 11
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component PillRow: Row {
        id: pills
        property var labels: []
        property var values: []
        property string current: ""
        signal picked(string value)
        spacing: 2
        Repeater {
            model: pills.labels
            delegate: Rectangle {
                property bool on: pills.values[index] === pills.current
                implicitHeight: 24
                implicitWidth: lab.implicitWidth + 20
                radius: 999
                color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12) : "transparent"
                Text {
                    id: lab
                    anchors.centerIn: parent
                    text: modelData
                    color: parent.on ? Theme.text : Theme.muted
                    font.pixelSize: 11
                    font.family: Theme.fontFamily
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pills.picked(pills.values[index])
                }
            }
        }
    }

    component SliderRow: Column {
        id: srow
        property string label: ""
        property real value: 0
        property real from: 0
        property real to: 100
        property real step: 1
        property real resetValue: 0
        signal changed(real v)
        width: parent ? parent.width : 280
        spacing: 0
        Rectangle {
            width: parent.width
            height: 1
            color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.06)
        }
        Item {
            width: parent.width
            height: 28
        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: srow.label
            color: Theme.text
            font.pixelSize: 11
            font.family: Theme.fontFamily
        }
        Text {
            id: resetGlyph
            anchors.left: parent.left
            anchors.leftMargin: Math.max(28, labMetrics.width + 8)
            anchors.verticalCenter: parent.verticalCenter
            text: "↺"
            color: Theme.muted
            font.pixelSize: 11
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: srow.changed(srow.resetValue)
            }
        }
        TextMetrics {
            id: labMetrics
            font.pixelSize: 11
            font.family: Theme.fontFamily
            text: srow.label
        }
        Text {
            id: valLabel
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            horizontalAlignment: Text.AlignRight
            text: String(Math.round(srow.value))
            color: Theme.muted
            font.pixelSize: 10
            font.family: Theme.fontFamily
        }
        Slider {
            id: sl
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: valLabel.left
            anchors.rightMargin: 6
            width: Math.max(70, parent.width * 0.42)
            from: srow.from
            to: srow.to
            stepSize: srow.step
            value: srow.value
            onMoved: srow.changed(value)
            background: Rectangle {
                x: sl.leftPadding
                y: sl.topPadding + sl.availableHeight / 2 - 2
                implicitWidth: 120
                implicitHeight: 4
                width: sl.availableWidth
                height: 4
                radius: 2
                color: Qt.rgba(1, 1, 1, 0.08)
                Rectangle {
                    width: sl.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                }
            }
            handle: Rectangle {
                x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
                y: sl.topPadding + sl.availableHeight / 2 - height / 2
                width: 12
                height: 12
                radius: 6
                color: Theme.accent
            }
        }
        }
    }

    component SectionBody: Item {
        width: col.width
        implicitHeight: inner.implicitHeight + 12
        height: implicitHeight
        default property alias extra: inner.data
        Column {
            id: inner
            x: 11
            y: 2
            width: parent.width - 22
            spacing: 0
        }
    }

    component GripDots: Item {
        width: 12
        height: 16
        Repeater {
            model: 6
            Rectangle {
                width: 2
                height: 2
                radius: 1
                color: Theme.muted
                opacity: 0.7
                x: (index % 2) * 5 + 2
                y: Math.floor(index / 2) * 5 + 2
            }
        }
    }

    component EyeButton: Item {
        id: eye
        property bool hidden: false
        signal clicked()
        width: 22
        height: 22
        Canvas {
            anchors.centerIn: parent
            width: 14
            height: 14
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = Theme.accent
                ctx.lineWidth = 1.4
                ctx.beginPath()
                ctx.moveTo(1, 7)
                ctx.quadraticCurveTo(7, 1, 13, 7)
                ctx.quadraticCurveTo(7, 13, 1, 7)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(7, 7, 2.2, 0, 6.3)
                ctx.stroke()
                if (eye.hidden) {
                    ctx.beginPath()
                    ctx.moveTo(2, 12)
                    ctx.lineTo(12, 2)
                    ctx.stroke()
                }
            }
        }
        onHiddenChanged: children[0].requestPaint()
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: eye.clicked()
        }
    }

    component MiniCheck: Item {
        id: box
        property bool checked: false
        signal toggled()
        width: 16
        height: 16
        Rectangle {
            anchors.fill: parent
            radius: 3
            color: box.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.88) : "transparent"
            border.width: 1
            border.color: box.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.28)
            Canvas {
                visible: box.checked
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = Qt.rgba(16 / 255, 21 / 255, 30 / 255, 1)
                    ctx.lineWidth = 1.6
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(3.2, 8.2)
                    ctx.lineTo(6.4, 11.2)
                    ctx.lineTo(12.6, 4.4)
                    ctx.stroke()
                }
                Component.onCompleted: requestPaint()
                onVisibleChanged: if (visible) requestPaint()
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: box.toggled()
        }
    }

    component SearchField: TextField {
        id: search
        implicitHeight: 30
        height: 30
        width: parent ? parent.width : 280
        color: Theme.text
        placeholderTextColor: Theme.muted
        font.pixelSize: 11
        font.family: Theme.fontFamily
        leftPadding: 12
        rightPadding: 10
        background: Rectangle {
            radius: 8
            color: Qt.rgba(4 / 255, 8 / 255, 13 / 255, 0.55)
            border.color: Theme.line
            border.width: 1
        }
    }

    component HubCard: Rectangle {
        id: card
        property string mode: ""
        property string title: ""
        property string desc: ""
        readonly property bool selected: (app.settings.hubMode || "local") === mode
        width: parent ? parent.width : 280
        implicitHeight: textCol.implicitHeight + 12
        height: implicitHeight
        radius: 10
        color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.07) : Qt.rgba(1, 1, 1, 0.05)
        border.width: 1
        border.color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.10)
        Row {
            anchors.fill: parent
            anchors.margins: 7
            spacing: 8
            Rectangle {
                width: 14
                height: 14
                radius: 7
                y: 2
                color: "transparent"
                border.width: 1.5
                border.color: card.selected ? Theme.accent : Theme.muted
                Rectangle {
                    visible: card.selected
                    anchors.centerIn: parent
                    width: 7
                    height: 7
                    radius: 4
                    color: Theme.accent
                }
            }
            Column {
                id: textCol
                width: parent.width - 22
                spacing: 2
                Text {
                    width: parent.width
                    text: card.title
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    text: card.desc
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    wrapMode: Text.WordWrap
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: app.updateSetting("hubMode", card.mode)
        }
    }

    Component {
        id: generalComp
        SectionBody {
            SettingRow {
                hairline: false
                label: panel.tr("settings.language.label", "Interface language")
                GlassCombo {
                    model: [
                        panel.tr("settings.language.auto", "Auto (system)"),
                        panel.tr("settings.language.english", "English"),
                        panel.tr("settings.language.zhTW", "繁體中文"),
                        panel.tr("settings.language.zhCN", "简体中文"),
                        panel.tr("settings.language.korean", "한국어"),
                        panel.tr("settings.language.japanese", "日本語")
                    ]
                    property var values: ["auto", "en", "zh-TW", "zh-CN", "ko", "ja"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.language || "auto"))
                    onActivated: app.updateSetting("language", values[currentIndex])
                }
            }
            SubHead { title: panel.tr("settings.startup.title", "Startup") }
            SettingRow {
                hairline: false
                label: panel.tr("settings.startup.startAtLogin", "Start at login")
                SwitchToggle {
                    checked: !!app.settings.startAtLogin
                    onToggled: function(on) { app.updateSetting("startAtLogin", on) }
                }
            }
            SubHead {
                title: panel.tr("settings.appUpdate.title", "App Updates")
                meta: panel.tr("settings.appUpdate.source", "GitHub releases")
            }
            SettingRow {
                hairline: false
                label: panel.tr("settings.appUpdate.automatic", "Download updates automatically")
                SwitchToggle {
                    checked: !!app.settings.automaticAppUpdates
                    onToggled: function(on) { app.updateSetting("automaticAppUpdates", on) }
                }
            }
            Item {
                width: parent.width
                height: 36
                Text {
                    text: panel.tr("settings.appUpdate.installed", "Installed")
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                }
                Text {
                    y: 16
                    text: Qt.application.version || "0.57.0"
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                }
                Text {
                    x: parent.width / 2
                    text: panel.tr("settings.appUpdate.latest", "Latest")
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                }
                Text {
                    x: parent.width / 2
                    y: 16
                    text: app.appUpdateLatest.length > 0 ? app.appUpdateLatest
                        : panel.tr("settings.common.notChecked", "Not checked")
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                }
            }
            Row {
                width: parent.width
                spacing: 8
                GlassButton {
                    text: panel.tr("settings.appUpdate.check", "Check for updates")
                    onClicked: app.checkUpdates()
                }
                GlassButton {
                    visible: app.appUpdateReady
                    text: panel.tr("settings.appUpdate.viewRelease", "View release")
                    onClicked: app.openUpdate()
                }
            }
            SubHead { title: panel.tr("settings.integrations.title", "Integrations") }
            SettingRow {
                hairline: false
                label: panel.tr("settings.integrations.discord", "Discord Rich Presence")
                desc: panel.tr("settings.integrations.discordDescription", "Show today's tokens, cost, and most-used AI tool in your Discord activity.")
                SwitchToggle {
                    checked: !!app.settings.discordRpcEnabled
                    onToggled: function(on) { app.updateSetting("discordRpcEnabled", on) }
                }
            }
            SubHead {
                title: panel.tr("settings.about.title", "About Token Monitor")
                meta: Qt.application.version || "0.57.0"
            }
            Text {
                width: parent.width
                text: panel.tr("settings.about.description", "Open-source AI tool token usage monitor, licensed under MIT.")
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 12
                Text {
                    text: panel.tr("settings.about.repository", "GitHub")
                    color: Theme.accent
                    font.pixelSize: 11
                    font.family: Theme.fontFamily
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally("https://github.com/Javis603/token-monitor") }
                }
                Text {
                    text: panel.tr("settings.about.website", "Website")
                    color: Theme.accent
                    font.pixelSize: 11
                    font.family: Theme.fontFamily
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally("https://javis-ai.com/token-monitor/") }
                }
                Text {
                    text: panel.tr("settings.about.reportIssue", "Report an issue")
                    color: Theme.accent
                    font.pixelSize: 11
                    font.family: Theme.fontFamily
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally("https://github.com/Javis603/token-monitor/issues") }
                }
            }
        }
    }

    Component {
        id: mainComp
        SectionBody {
            SettingRow {
                hairline: false
                label: panel.tr("periodRange.settingsTitle", "Default usage range")
                desc: panel.tr("periodRange.settingsNote", "Choose the default usage range shown on Home.")
                GlassCombo {
                    model: [
                        panel.tr("periodRange.month", "This month"),
                        panel.tr("periodRange.week", "This week"),
                        panel.tr("periodRange.last7", "Last 7 days"),
                        panel.tr("periodRange.last30", "Last 30 days")
                    ]
                    property var values: ["month", "week", "last7", "last30"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.periodMonthMode || "month"))
                    onActivated: app.updateSetting("periodMonthMode", values[currentIndex])
                }
            }
            Item {
                width: parent.width
                implicitHeight: noteRow.height
                height: implicitHeight
                RowLayout {
                    id: noteRow
                    width: parent.width
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: panel.tr("settings.views.note", "Choose which main views appear and the order of the mode button.")
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: "↺"
                        color: Theme.accent
                        font.pixelSize: 13
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            cursorShape: Qt.PointingHandCursor
                            onClicked: app.resetViewDisplayOrder()
                        }
                    }
                    EyeButton {
                        hidden: false
                        onClicked: app.showAllViews()
                    }
                }
            }
            Column {
                width: parent.width
                spacing: 2
                Repeater {
                    model: { var _ = panel.summaryTick; return app.allViews() }
                    delegate: Column {
                        width: parent.width
                        spacing: 4
                        RowLayout {
                            width: parent.width
                            height: 20
                            spacing: 5
                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: app.viewLabelFor(modelData)
                                color: Theme.text
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                                opacity: app.viewIsHidden(modelData) ? 0.45 : 1
                            }
                            Item {
                                visible: modelData === "home" || modelData === "status" || modelData === "project" || modelData === "trends"
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                TintIcon {
                                    anchors.centerIn: parent
                                    size: 13
                                    source: app.uiIcon("settings/general.svg")
                                    tint: ((modelData === "home" && panel.homeOpen)
                                           || (modelData === "status" && panel.statusOpen)
                                           || (modelData === "project" && panel.projectOpen)
                                           || (modelData === "trends" && panel.trendOpen)) ? Theme.accent : Theme.muted
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (modelData === "home") panel.homeOpen = !panel.homeOpen
                                        else if (modelData === "status") panel.statusOpen = !panel.statusOpen
                                        else if (modelData === "project") panel.projectOpen = !panel.projectOpen
                                        else if (modelData === "trends") panel.trendOpen = !panel.trendOpen
                                    }
                                }
                            }
                            EyeButton {
                                hidden: app.viewIsHidden(modelData)
                                onClicked: app.toggleViewHidden(modelData)
                            }
                            GripDots {
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.SizeVerCursor
                                    property real startY: 0
                                    onPressed: function(mouse) { startY = mouse.y }
                                    onReleased: function(mouse) {
                                        var dy = mouse.y - startY
                                        if (dy > 10) app.moveViewDisplay(modelData, 1)
                                        else if (dy < -10) app.moveViewDisplay(modelData, -1)
                                    }
                                }
                            }
                        }
                        Column {
                            visible: modelData === "home" && panel.homeOpen
                            width: parent.width
                            spacing: 8
                            SettingRow {
                                hairline: false
                                label: panel.tr("settings.home.showLimitBars", "Show limit bars on Home")
                                SwitchToggle {
                                    checked: !!app.settings.showHomeLimitBars
                                    onToggled: function(on) { app.updateSetting("showHomeLimitBars", on) }
                                }
                            }
                            SettingRow {
                                label: panel.tr("settings.home.showProviderNames", "Show provider names on Home bars")
                                SwitchToggle {
                                    checked: !!app.settings.showHomeLimitProviderNames
                                    onToggled: function(on) { app.updateSetting("showHomeLimitProviderNames", on) }
                                }
                            }
                            SettingRow {
                                label: panel.tr("settings.home.heatmapColor", "Heatmap color")
                                PillRow {
                                    labels: [panel.tr("dashboard.heatmap.cost", "Cost"), panel.tr("dashboard.heatmap.tokens", "Tokens")]
                                    values: ["cost", "tokens"]
                                    current: app.settings.heatmapMetric || "cost"
                                    onPicked: function(v) { app.updateSetting("heatmapMetric", v) }
                                }
                            }
                        }
                    }
                }
            }
            SettingRow {
                label: panel.tr("settings.currency.label", "Currency")
                GlassCombo {
                    model: [
                        panel.tr("settings.currency.usd", "USD - US Dollar"),
                        panel.tr("settings.currency.twd", "TWD - New Taiwan Dollar"),
                        panel.tr("settings.currency.hkd", "HKD - Hong Kong Dollar"),
                        panel.tr("settings.currency.cny", "CNY - Chinese Yuan")
                    ]
                    property var values: ["USD", "TWD", "HKD", "CNY"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.currency || "USD"))
                    onActivated: app.updateSetting("currency", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.modelRanking.label", "Model ranking")
                PillRow {
                    labels: [panel.tr("settings.modelRanking.tokens", "Tokens"), panel.tr("settings.modelRanking.cost", "Cost")]
                    values: ["tokens", "cost"]
                    current: app.settings.modelRankingMetric || "tokens"
                    onPicked: function(v) { app.updateSetting("modelRankingMetric", v) }
                }
            }
            // Electron rate mode: a manual override is just a value in
            // currencyRates; deleting the key returns the code to auto.
            SettingRow {
                visible: (app.settings.currency || "USD") !== "USD"
                label: panel.tr("settings.currency.rateMode", "Exchange rate mode")
                PillRow {
                    labels: [panel.tr("settings.currency.modeAuto", "Auto"), panel.tr("settings.currency.modeManual", "Manual")]
                    values: ["auto", "manual"]
                    current: Number(app.settings.currencyRates && app.settings.currencyRates[app.settings.currency]) > 0 ? "manual" : "auto"
                    onPicked: function(v) {
                        var rates = Object.assign({}, app.settings.currencyRates || {})
                        if (v === "manual") {
                            var eff = Number(rates[app.settings.currency]) || 1
                            rates[app.settings.currency] = eff
                        } else {
                            delete rates[app.settings.currency]
                        }
                        app.updateSetting("currencyRates", rates)
                    }
                }
            }
            TextField {
                visible: (app.settings.currency || "USD") !== "USD"
                         && Number(app.settings.currencyRates && app.settings.currencyRates[app.settings.currency]) > 0
                width: parent.width
                placeholderText: panel.tr("settings.currency.ratePlaceholder", "Rate per 1 USD")
                text: {
                    var v = Number(app.settings.currencyRates && app.settings.currencyRates[app.settings.currency])
                    return isFinite(v) && v > 0 ? String(Number(v.toFixed(v >= 1 ? 2 : 4))) : ""
                }
                color: Theme.text
                font.pixelSize: 11
                font.family: Theme.fontFamily
                onEditingFinished: {
                    var rates = Object.assign({}, app.settings.currencyRates || {})
                    var num = Number(text)
                    if (isFinite(num) && num > 0) rates[app.settings.currency] = num
                    else delete rates[app.settings.currency]
                    app.updateSetting("currencyRates", rates)
                }
                background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
            }
            SettingRow {
                label: panel.tr("settings.home.limitAccounts", "Home limit accounts")
                GlassCombo {
                    model: ["1", "2", "3", "4", "5", "6"]
                    property var values: [1, 2, 3, 4, 5, 6]
                    currentIndex: Math.max(0, values.indexOf(Number(app.settings.homeLimitAccountCount || 3)))
                    onActivated: app.updateSetting("homeLimitAccountCount", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.home.activeDaysWindow", "Active days window")
                PillRow {
                    labels: [panel.tr("settings.home.activeDaysAll", "All"), panel.tr("settings.home.activeDaysYear", "Past year")]
                    values: ["all", "year"]
                    current: app.settings.homeActiveDaysWindow || "all"
                    onPicked: function(v) { app.updateSetting("homeActiveDaysWindow", v) }
                }
            }
        }
    }

    Component {
        id: windowComp
        SectionBody {
            SettingRow {
                hairline: false
                label: panel.tr("settings.display.windowBehavior", "Window behavior")
                desc: panel.tr("settings.display.windowBehaviorNote", "Desktop pinned keeps the widget below other apps.")
                GlassCombo {
                    model: [
                        panel.tr("settings.windowBehavior.floating", "Floating above apps"),
                        panel.tr("settings.windowBehavior.normal", "Normal window"),
                        panel.tr("settings.windowBehavior.desktop", "Desktop pinned")
                    ]
                    property var values: ["floating", "normal", "desktop"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.windowBehavior || "floating"))
                    onActivated: app.updateSetting("windowBehavior", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.display.keepAboveTaskbar", "Keep above taskbar (Experimental)")
                SwitchToggle {
                    checked: !!app.settings.keepAboveTaskbar
                    onToggled: function(on) { app.updateSetting("keepAboveTaskbar", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.display.trayIcon", "Tray icon")
                SwitchToggle {
                    checked: app.settings.showTrayIcon !== false
                    onToggled: function(on) { app.updateSetting("showTrayIcon", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.display.trayText", "Tray display")
                GlassCombo {
                    model: [
                        panel.tr("settings.tray.tokens", "Today tokens"),
                        panel.tr("settings.tray.cost", "Today cost"),
                        panel.tr("settings.tray.both", "Today tokens & cost"),
                        panel.tr("settings.tray.tokensAll", "Total tokens"),
                        panel.tr("settings.tray.costAll", "Total cost"),
                        panel.tr("settings.tray.bothAll", "Total tokens & cost"),
                        panel.tr("settings.tray.limitsAllSessions", "AI tool limits"),
                        panel.tr("settings.tray.icon", "App icon only")
                    ]
                    property var values: ["tokens", "cost", "both", "tokensAll", "costAll", "bothAll", "limitsAllSessions", "icon"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.trayContent || "tokens"))
                    onActivated: app.updateSetting("trayContent", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.display.hideAppIcon", "Hide app icon")
                desc: panel.tr("settings.display.hideAppIconNote", "Keep the widget off the taskbar and Alt+Tab; the tray stays the launcher.")
                SwitchToggle {
                    checked: !!app.settings.hideAppIcon
                    onToggled: function(on) { app.updateSetting("hideAppIcon", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.display.topEdgeHide", "Top-edge hide")
                SwitchToggle {
                    checked: !!app.settings.topEdgeHideEnabled
                    onToggled: function(on) { app.updateSetting("topEdgeHideEnabled", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.display.floatingBubble", "Floating bubble")
                SwitchToggle {
                    checked: !!app.settings.floatingBubbleEnabled
                    onToggled: function(on) { app.updateSetting("floatingBubbleEnabled", on) }
                }
            }
            SettingRow {
                visible: !!app.settings.floatingBubbleEnabled
                label: panel.tr("settings.display.floatingBubbleContent", "Bubble content")
                PillRow {
                    labels: [panel.tr("settings.bubble.icon", "Icon"), panel.tr("settings.bubble.tokens", "Tokens")]
                    values: ["icon", "tokens"]
                    current: app.settings.floatingBubbleContent || "tokens"
                    onPicked: function(v) { app.updateSetting("floatingBubbleContent", v) }
                }
            }
        }
    }

    Component {
        id: appearanceComp
        SectionBody {
            SettingRow {
                hairline: false
                label: panel.tr("settings.appearance.systemGlass", "Glass Backdrop")
                PillRow {
                    labels: [panel.tr("settings.appearance.glassEffectSystem", "System"), panel.tr("settings.appearance.glassEffectTransparent", "Transparent")]
                    values: ["system", "off"]
                    current: app.settings.systemGlass === false ? "off" : "system"
                    onPicked: function(v) { app.updateSetting("systemGlass", v !== "off") }
                }
            }
            SettingRow {
                visible: app.settings.systemGlass !== false
                label: panel.tr("settings.appearance.windowsBackdrop", "Windows Glass Style")
                GlassCombo {
                    model: [
                        panel.tr("settings.appearance.windowsBackdropAcrylic", "Acrylic"),
                        panel.tr("settings.appearance.windowsBackdropAccent", "Accent Blur (experimental)")
                    ]
                    property var values: ["acrylic", "accent"]
                    currentIndex: app.settings.windowsBackdrop === "accent" ? 1 : 0
                    onActivated: app.updateSetting("windowsBackdrop", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.liveIndicator", "Live Indicator")
                SwitchToggle {
                    checked: app.settings.showLiveDot !== false
                    onToggled: function(on) { app.updateSetting("showLiveDot", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.toolIcons", "Tool Icons")
                SwitchToggle {
                    checked: app.settings.showToolIcons !== false
                    onToggled: function(on) { app.updateSetting("showToolIcons", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.titleIcon", "Use Title Icon")
                SwitchToggle {
                    checked: app.settings.titleIconOnly !== false
                    onToggled: function(on) { app.updateSetting("titleIconOnly", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.compactTotalTokens", "Show compact token total")
                SwitchToggle {
                    checked: !!app.settings.showCompactTotalTokens
                    onToggled: function(on) { app.updateSetting("showCompactTotalTokens", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.liveTokenRate", "Show live token rate")
                desc: panel.tr("settings.appearance.liveTokenRateNote", "Show the latest generation speed in the footer.")
                SwitchToggle {
                    checked: !!app.settings.showLiveTokenRate
                    onToggled: function(on) { app.updateSetting("showLiveTokenRate", on) }
                }
            }
            SettingRow {
                visible: !!app.settings.showLiveTokenRate && (app.settings.hubMode === "client" || app.settings.hubMode === "host")
                label: panel.tr("settings.appearance.liveTokenRateScope", "Live rate scope")
                GlassCombo {
                    implicitWidth: 120
                    model: [panel.tr("settings.appearance.liveTokenRateScopeAll", "All devices"), panel.tr("settings.appearance.liveTokenRateScopeDevice", "This device")]
                    property var values: ["all", "device"]
                    currentIndex: app.settings.liveTokenRateScope === "device" ? 1 : 0
                    onActivated: app.updateSetting("liveTokenRateScope", values[currentIndex])
                }
            }
            SettingRow {
                visible: !!app.settings.showCompactTotalTokens
                label: panel.tr("settings.appearance.compactTokenUnits", "Compact token units")
                GlassCombo {
                    model: [panel.tr("settings.appearance.compactTokenUnitsWestern", "International (K/M/B)"), panel.tr("settings.appearance.compactTokenUnitsLocalized", "East Asian")]
                    property var values: ["western", "localized"]
                    currentIndex: app.settings.compactTokenUnits === "localized" ? 1 : 0
                    onActivated: app.updateSetting("compactTokenUnits", values[currentIndex])
                }
            }
            SettingRow {
                label: panel.tr("settings.appearance.swapSettingsRefresh", "Swap Settings and Refresh buttons")
                SwitchToggle {
                    checked: !!app.settings.settingsInTitlebar
                    onToggled: function(on) { app.updateSetting("settingsInTitlebar", on) }
                }
            }
            SliderRow {
                label: panel.tr("settings.appearance.glass", "Glass")
                value: Number(app.settings.glassOpacity || 68)
                resetValue: 68
                onChanged: function(v) { app.updateSetting("glassOpacity", Math.round(v)) }
            }
            SliderRow {
                label: panel.tr("settings.appearance.depth", "Depth")
                value: Number(app.settings.glassBlur || 32)
                resetValue: 32
                onChanged: function(v) { app.updateSetting("glassBlur", Math.round(v)) }
            }
            SliderRow {
                label: panel.tr("settings.appearance.zoom", "Zoom")
                from: 70
                to: 160
                step: 5
                value: Math.round(Number(app.settings.zoomFactor || 1) * 100)
                resetValue: 100
                onChanged: function(v) { app.updateSetting("zoomFactor", v / 100) }
            }
            SettingRow {
                label: panel.tr("settings.appearance.reduceMotion", "Reduce Motion")
                desc: panel.tr("settings.appearance.reduceMotionNote", "Reduce animations, or follow the system setting.")
                PillRow {
                    labels: [
                        panel.tr("settings.appearance.motion.system", "System"),
                        panel.tr("settings.appearance.motion.on", "On"),
                        panel.tr("settings.appearance.motion.off", "Off")
                    ]
                    values: ["system", "on", "off"]
                    current: app.settings.reduceMotion || "system"
                    onPicked: function(v) { app.updateSetting("reduceMotion", v) }
                }
            }
            // Electron fontSettings: presets resolve to a CSS family list —
            // 'app' keeps the built-in stack, system/mono pick a concrete
            // family, custom stores the typed list verbatim.
            SettingRow {
                label: panel.tr("settings.appearance.fontInterface", "Interface font")
                GlassCombo {
                    model: [
                        panel.tr("settings.appearance.fontPresetApp", "App default"),
                        panel.tr("settings.appearance.fontPresetSystem", "System"),
                        panel.tr("settings.appearance.fontPresetMono", "Monospace"),
                        panel.tr("settings.appearance.fontPresetCustom", "Custom")
                    ]
                    property string current: app.settings.interfaceFontFamily || ""
                    property int presetIndex: current.length === 0 ? 0
                        : current === "Segoe UI" ? 1
                        : current.indexOf("Consolas") >= 0 ? 2
                        : 3
                    currentIndex: Math.max(0, presetIndex)
                    onActivated: {
                        if (currentIndex === 0) app.updateSetting("interfaceFontFamily", "")
                        else if (currentIndex === 1) app.updateSetting("interfaceFontFamily", "Segoe UI")
                        else if (currentIndex === 2) app.updateSetting("interfaceFontFamily", "Consolas, Cascadia Mono, Cascadia Code")
                        // Custom keeps the field open; the text field below writes it.
                    }
                }
            }
            TextField {
                visible: (app.settings.interfaceFontFamily || "").length > 0
                         && app.settings.interfaceFontFamily !== "Segoe UI"
                         && app.settings.interfaceFontFamily.indexOf("Consolas") < 0
                width: parent.width
                placeholderText: panel.tr("settings.appearance.fontCustomLabel", "Custom font family list")
                text: app.settings.interfaceFontFamily || ""
                color: Theme.text
                font.pixelSize: 11
                font.family: Theme.fontFamily
                onEditingFinished: app.updateSetting("interfaceFontFamily", text)
                background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
            }
            SettingRow {
                label: panel.tr("settings.appearance.fontDisplay", "Display font")
                GlassCombo {
                    model: [
                        panel.tr("settings.appearance.fontPresetFollow", "Follow interface"),
                        panel.tr("settings.appearance.fontPresetSystem", "System"),
                        panel.tr("settings.appearance.fontPresetCustom", "Custom")
                    ]
                    property string current: app.settings.displayFontFamily || ""
                    property int presetIndex: current.length === 0 || current === "ui-monospace" ? 0
                        : current === "Segoe UI" ? 1
                        : 2
                    currentIndex: Math.max(0, presetIndex)
                    onActivated: {
                        if (currentIndex === 0) app.updateSetting("displayFontFamily", "")
                        else if (currentIndex === 1) app.updateSetting("displayFontFamily", "Segoe UI")
                    }
                }
            }
            Item { width: parent.width; height: 6 }
            Item {
                width: parent.width
                height: 18
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: panel.tr("settings.appearance.themeTitle", "Interface Theme")
                    color: Theme.muted
                    font.pixelSize: 9
                    font.letterSpacing: 1.2
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "↺"
                    color: Theme.accent
                    font.pixelSize: 13
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.updateSetting("themeColors", { preset: "default" })
                    }
                }
            }
            Row {
                spacing: 6
                Repeater {
                    model: [
                        { id: "default", key: "settings.appearance.preset.default", label: "Default" },
                        { id: "obsidian", key: "settings.appearance.preset.obsidian", label: "Obsidian" },
                        { id: "porcelain", key: "settings.appearance.preset.porcelain", label: "Porcelain" }
                    ]
                    delegate: Rectangle {
                        property bool on: (app.theme.preset || "default") === modelData.id
                        implicitHeight: 24
                        implicitWidth: chipLab.implicitWidth + 18
                        radius: 999
                        color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12) : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        border.color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45) : Theme.line
                        Text {
                            id: chipLab
                            anchors.centerIn: parent
                            text: panel.tr(modelData.key, modelData.label)
                            color: parent.on ? Theme.accent : Theme.muted
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: app.updateSetting("themeColors", { preset: modelData.id })
                        }
                    }
                }
            }
        }
    }

    Component {
        id: collectionComp
        SectionBody {
            id: coll
            property string query: ""
            property var rows: []
            property var expanded: ({})
            function refreshRows() {
                var all = app.catalogClientRows()
                var q = String(coll.query || "").trim().toLowerCase()
                if (!q) {
                    coll.rows = all
                    return
                }
                var out = []
                for (var i = 0; i < all.length; ++i) {
                    var r = all[i]
                    var hay = (String(r.label || "") + " " + String(r.id || "")).toLowerCase()
                    if (hay.indexOf(q) >= 0) out.push(r)
                }
                coll.rows = out
            }
            Connections {
                target: app
                function onSettingsChanged() { coll.refreshRows() }
                function onStatsChanged() { coll.refreshRows() }
            }
            Component.onCompleted: refreshRows()

            Item {
                width: parent.width
                implicitHeight: Math.max(noteLab.height, 22)
                height: implicitHeight
                Text {
                    id: noteLab
                    width: parent.width - 52
                    text: panel.tr("settings.tools.note", "Track controls collection. Show controls the main Tools list. Drag a row to reorder.")
                    color: Theme.muted
                    font.pixelSize: 10
                    font.family: Theme.fontFamily
                    wrapMode: Text.Wrap
                }
                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 2
                    Text {
                        text: "↺"
                        color: Theme.muted
                        font.pixelSize: 13
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: app.resetClientDisplayOrder()
                        }
                    }
                    EyeButton {
                        hidden: false
                        onClicked: app.showAllClients()
                    }
                }
            }
            SearchField {
                placeholderText: panel.tr("settings.tools.search", "Search tools")
                onTextChanged: {
                    coll.query = text
                    coll.refreshRows()
                }
            }
            Repeater {
                model: coll.rows
                delegate: Column {
                    width: coll.width > 22 ? coll.width - 22 : parent.width
                    spacing: 0
                    Item {
                        width: parent.width
                        height: 32
                        MiniCheck {
                            id: trackBox
                            y: 8
                            checked: !!modelData.tracked
                            onToggled: app.toggleClientTracked(modelData.id)
                        }
                        Text {
                            id: clientName
                            x: 22
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, parent.width - 92)
                            text: modelData.label || modelData.id
                            color: Theme.text
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            visible: !!modelData.tracked
                            x: clientName.x + clientName.width + 6
                            anchors.verticalCenter: parent.verticalCenter
                            width: tagLab.width + 10
                            height: 16
                            radius: 5
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                            Text {
                                id: tagLab
                                anchors.centerIn: parent
                                text: panel.tr("settings.tools.status.active", "Tracked")
                                color: Theme.accent
                                font.pixelSize: 9
                                font.family: Theme.fontFamily
                            }
                        }
                        EyeButton {
                            anchors.right: parent.right
                            anchors.rightMargin: 18
                            anchors.verticalCenter: parent.verticalCenter
                            hidden: !!modelData.hidden
                            onClicked: app.toggleClientHidden(modelData.id)
                        }
                        Canvas {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8
                            height: 8
                            rotation: coll.expanded[modelData.id] ? 180 : 0
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = Theme.muted
                                ctx.lineWidth = 1.4
                                ctx.beginPath()
                                ctx.moveTo(1, 3)
                                ctx.lineTo(4, 6)
                                ctx.lineTo(7, 3)
                                ctx.stroke()
                            }
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -6
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var next = Object.assign({}, coll.expanded)
                                    next[modelData.id] = !coll.expanded[modelData.id]
                                    coll.expanded = next
                                }
                            }
                        }
                    }
                    Text {
                        visible: !!coll.expanded[modelData.id]
                        width: parent.width
                        leftPadding: 22
                        bottomPadding: 8
                        text: modelData.id
                        color: Theme.muted
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                    }
                }
            }
            SettingRow {
                hairline: true
                label: panel.tr("settings.collection.mode.label", "Collection mode")
                desc: panel.tr("settings.collection.modeDesc", "Live watches tool data and refreshes; smart backs off to a fixed cadence; interval runs on the schedule alone.")
                GlassCombo {
                    model: [
                        panel.tr("settings.collection.mode.live", "Live (watch + interval)"),
                        panel.tr("settings.collection.mode.smart", "Smart (watch + fixed cadence)"),
                        panel.tr("settings.collection.mode.interval", "Interval only")
                    ]
                    property var values: ["live", "smart", "interval"]
                    currentIndex: Math.max(0, values.indexOf(app.settings.collectionMode || "live"))
                    onActivated: app.updateSetting("collectionMode", values[currentIndex])
                }
            }
            SettingRow {
                visible: (app.settings.collectionMode || "live") !== "smart"
                label: panel.tr("settings.collection.cadence", "Collection cadence")
                GlassCombo {
                    model: [
                        panel.tr("settings.collection.interval.5m", "Every 5 minutes"),
                        panel.tr("settings.collection.interval.15m", "Every 15 minutes"),
                        panel.tr("settings.collection.interval.30m", "Every 30 minutes")
                    ]
                    property var values: [300000, 900000, 1800000]
                    currentIndex: Math.max(0, values.indexOf(Number(app.settings.collectionIntervalMs || 300000)))
                    onActivated: app.updateSetting("collectionIntervalMs", values[currentIndex])
                }
            }
            SettingRow {
                hairline: true
                label: panel.tr("settings.collection.wslScan", "Scan tools inside WSL")
                SwitchToggle {
                    checked: app.settings.wslScanEnabled !== false
                    onToggled: function(on) { app.updateSetting("wslScanEnabled", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.collection.history", "History archive")
                SwitchToggle {
                    checked: app.settings.historyEnabled !== false
                    onToggled: function(on) { app.updateSetting("historyEnabled", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.collection.projects", "Projects")
                SwitchToggle {
                    checked: app.settings.projectsEnabled !== false
                    onToggled: function(on) { app.updateSetting("projectsEnabled", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.export.autoLabel", "Auto export")
                desc: panel.tr("settings.export.desc", "Write the CSV + JSON snapshot to the export folder on a fixed interval.")
                SwitchToggle {
                    checked: !!app.settings.exportAutoEnabled
                    onToggled: function(on) { app.updateSetting("exportAutoEnabled", on) }
                }
            }
            SettingRow {
                visible: !!app.settings.exportAutoEnabled
                label: panel.tr("settings.export.interval", "Export interval")
                GlassCombo {
                    model: ["1 min", "5 min", "15 min", "30 min", "60 min"]
                    property var values: [60000, 300000, 900000, 1800000, 3600000]
                    currentIndex: Math.max(0, values.indexOf(Number(app.settings.exportIntervalMs || 60000)))
                    onActivated: app.updateSetting("exportIntervalMs", values[currentIndex])
                }
            }
            SettingRow {
                visible: !!app.settings.exportAutoEnabled
                label: panel.tr("settings.export.dir", "Export folder")
                GlassButton {
                    text: panel.tr("settings.export.pickDir", "Choose…")
                    onClicked: app.pickExportDir()
                }
            }
            Text {
                visible: !!app.settings.exportAutoEnabled && String(app.settings.exportDir || "").length > 0
                width: parent.width
                text: app.settings.exportDir || ""
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                elide: Text.ElideMiddle
            }
            GlassButton { text: panel.tr("settings.export.now", "Export now"); onClicked: app.exportNow() }
            GlassButton { text: panel.tr("settings.about.diagnostics.generate", "Generate report"); onClicked: app.exportDiagnostics() }
        }
    }

    Component {
        id: limitsComp
        SectionBody {
            id: lim
            property string query: ""
            property var rows: []
            // Per-provider credential fields, mirroring Electron's account
            // accordions (key names = CREDENTIAL_SETTING_PATHS in both apps).
            property var credentialFields: ({
                claude: [{ key: "claudeWebCookie", label: "Web Cookie", secret: true }],
                deepseek: [{ key: "deepseekApiKey", label: "API Key", secret: true }],
                minimax: [{ key: "minimaxApiKey", label: "API Key", secret: true }],
                kimi: [{ key: "kimiApiKey", label: "API Key", secret: true }, { key: "kimiWebAccessToken", label: "Web Access Token", secret: true }],
                copilot: [{ key: "copilotApiToken", label: "API Token", secret: true }, { key: "copilotEnterpriseHost", label: "Enterprise Host", secret: false }],
                zai: [{ key: "zaiApiKey", label: "API Key", secret: true }],
                zaiteam: [{ key: "zaiTeamApiKey", label: "Team API Key", secret: true }, { key: "zaiTeamOrganizationId", label: "Organization ID", secret: false }, { key: "zaiTeamProjectId", label: "Project ID", secret: false }],
                volcengine: [{ key: "volcengineAccessKeyId", label: "Access Key ID", secret: true }, { key: "volcengineSecretAccessKey", label: "Secret Access Key", secret: true }, { key: "volcengineAgentAccessKeyId", label: "Agent Access Key ID", secret: true }, { key: "volcengineAgentSecretAccessKey", label: "Agent Secret Access Key", secret: true }],
                alibaba: [{ key: "alibabaCookie", label: "Cookie", secret: true }],
                qoder: [{ key: "qoderCookie", label: "Cookie", secret: true }],
                trae: [{ key: "traeAccessToken", label: "Access Token", secret: true }, { key: "traeDeviceId", label: "Device ID", secret: false }],
                zed: [{ key: "zedCookie", label: "Cookie", secret: true }],
                commandcode: [{ key: "commandcodeCookie", label: "Cookie", secret: true }],
                ollama: [{ key: "ollamaCookie", label: "Cookie", secret: true }],
                opencode: [{ key: "opencodeCookie", label: "Cookie", secret: true }]
            })
            function credentialIsSet(key) {
                return String(app.settings[key] || "") === "set"
            }
            function credentialsSet(id) {
                var fields = credentialFields[id] || []
                for (var i = 0; i < fields.length; ++i)
                    if (credentialIsSet(String(fields[i].key))) return true
                return false
            }
            function tagText(t) {
                var lang = String(app.i18n.resolvedLanguage || "")
                if (lang.indexOf("zh") === 0) {
                    if (t === "Auto") return "自动"
                    if (t === "Manual login") return "手动登录"
                    if (t === "Pay-as-you-go") return "按量"
                }
                return t
            }
            function refreshRows() {
                var all = app.catalogLimitRows()
                var q = String(lim.query || "").trim().toLowerCase()
                if (!q) {
                    lim.rows = all
                    return
                }
                var out = []
                for (var i = 0; i < all.length; ++i) {
                    var r = all[i]
                    var hay = (String(r.label || "") + " " + String(r.id || "")).toLowerCase()
                    if (hay.indexOf(q) >= 0) out.push(r)
                }
                lim.rows = out
            }
            Connections {
                target: app
                function onSettingsChanged() { lim.refreshRows() }
                function onStatsChanged() { lim.refreshRows() }
            }
            Component.onCompleted: refreshRows()

            Text {
                width: parent.width
                text: panel.tr("settings.limits.reorderNote", "Drag a provider to reorder the list.")
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
            SearchField {
                placeholderText: panel.tr("settings.limits.search", "Search providers")
                onTextChanged: {
                    lim.query = text
                    lim.refreshRows()
                }
            }
            Repeater {
                model: lim.rows
                delegate: Item {
                    width: parent.width
                    height: 40
                    MiniCheck {
                        y: 12
                        checked: !!modelData.enabled
                        onToggled: app.toggleLimitProvider(modelData.id)
                    }
                    Column {
                        x: 22
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 110
                        spacing: 2
                        Text {
                            text: modelData.label || modelData.id
                            color: Theme.text
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                            width: parent.width
                        }
                        Row {
                            spacing: 4
                            Repeater {
                                model: modelData.tags || []
                                Text {
                                    text: lim.tagText(modelData)
                                    color: Theme.muted
                                    font.pixelSize: 9
                                    font.family: Theme.fontFamily
                                }
                            }
                        }
                    }
                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        Rectangle {
                            visible: String(modelData.statusLabel || "").indexOf("已连接") >= 0
                                     || String(modelData.statusLabel || "").toLowerCase().indexOf("connected") >= 0
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.accent
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.statusLabel || ""
                            color: Theme.muted
                            font.pixelSize: 10
                            font.family: Theme.fontFamily
                        }
                    }
                }
            }
            SettingRow {
                hairline: true
                label: panel.tr("settings.limits.enabled", "Limits enabled")
                SwitchToggle {
                    checked: app.settings.limitsEnabled !== false
                    onToggled: function(on) { app.updateSetting("limitsEnabled", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.limits.showSource", "Show limit source")
                SwitchToggle {
                    checked: !!app.settings.showLimitSource
                    onToggled: function(on) { app.updateSetting("showLimitSource", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.limits.showUsed", "Show used instead of remaining")
                SwitchToggle {
                    checked: !!app.settings.showLimitUsed
                    onToggled: function(on) { app.updateSetting("showLimitUsed", on) }
                }
            }
            SettingRow {
                label: panel.tr("settings.limits.refreshMode", "Refresh mode")
                PillRow {
                    labels: [panel.tr("settings.limits.refreshFixed", "Fixed"), panel.tr("settings.limits.refreshAdaptive", "Adaptive")]
                    values: ["fixed", "adaptive"]
                    current: app.settings.limitsRefreshMode || "fixed"
                    onPicked: function(v) { app.updateSetting("limitsRefreshMode", v) }
                }
            }
            SettingRow {
                label: panel.tr("settings.limits.refreshInterval", "Refresh interval")
                GlassCombo {
                    model: ["1 min", "2 min", "5 min", "15 min", "30 min"]
                    property var values: [60000, 120000, 300000, 900000, 1800000]
                    currentIndex: Math.max(0, values.indexOf(Number(app.settings.limitsRefreshMs || 300000)))
                    onActivated: app.updateSetting("limitsRefreshMs", values[currentIndex])
                }
            }
            // Electron's per-provider account accordions, condensed: the fields
            // each provider's collector reads, saved straight into the shared
            // credential store. Login-flow providers (Codex OAuth, Cursor
            // multi-account, MiMo managed accounts) stay env/CLI-managed.
            Repeater {
                model: {
                    var out = []
                    var all = app.catalogLimitRows()
                    for (var i = 0; i < all.length; ++i) {
                        var id = String(all[i].id || "")
                        if (lim.credentialFields[id]) out.push(all[i])
                    }
                    return out
                }
                delegate: Column {
                    width: parent.width
                    spacing: 6
                    property var fields: lim.credentialFields[String(modelData.id)] || []
                    property bool expanded: false
                    Item {
                        width: parent.width
                        height: 24
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: (modelData.label || modelData.id) + " · " + panel.tr("settings.limits.credentials", "Credentials")
                            color: Theme.muted
                            font.pixelSize: 10
                            font.family: Theme.fontFamily
                        }
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Rectangle {
                                visible: lim.credentialsSet(String(modelData.id))
                                width: 6; height: 6; radius: 3
                                color: Theme.accent
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: expanded ? "▴" : "▾"
                                color: Theme.muted
                                font.pixelSize: 10
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: expanded = !expanded
                        }
                    }
                    Column {
                        visible: expanded
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: fields
                            delegate: Column {
                                width: parent.width
                                spacing: 3
                                property var field: modelData
                                property string draft: ""
                                Text {
                                    text: field.label
                                    color: Theme.muted
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                }
                                TextField {
                                    width: parent.width
                                    echoMode: field.secret ? TextInput.Password : TextInput.Normal
                                    placeholderText: lim.credentialIsSet(String(field.key))
                                        ? panel.tr("settings.limits.credentialSet", "Saved — type to replace")
                                        : panel.tr("settings.limits.credentialEmpty", "Not set")
                                    text: draft
                                    color: Theme.text
                                    font.pixelSize: 11
                                    font.family: Theme.fontFamily
                                    onEditingFinished: {
                                        app.updateSetting(String(field.key), text)
                                        draft = ""
                                    }
                                    background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: subsComp
        SectionBody {
            id: subs
            property bool adding: false
            Text {
                width: parent.width
                text: panel.tr("settings.subscriptions.note", "Record what you actually pay for each AI account. Hover an account's plan label on the AI Tool Limits page to see what you recorded here.")
                color: Theme.muted
                font.pixelSize: 10
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: app.subscriptions
                SettingRow {
                    hairline: index > 0
                    label: (modelData.name || "sub") + "  " + Number(modelData.amount || 0).toFixed(2)
                    GlassButton { text: panel.tr("settings.common.remove", "Remove"); onClicked: app.removeSubscription(index) }
                }
            }
            Text {
                visible: !(app.subscriptions && app.subscriptions.length)
                width: parent.width
                topPadding: 8
                bottomPadding: 8
                horizontalAlignment: Text.AlignHCenter
                text: panel.tr("settings.subscriptions.emptyList", "No subscriptions yet")
                color: Theme.muted
                font.pixelSize: 11
                font.family: Theme.fontFamily
            }
            Rectangle {
                width: parent.width
                implicitHeight: addInner.implicitHeight + 16
                height: implicitHeight
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.03)
                border.width: 1
                border.color: Qt.rgba(232 / 255, 238 / 255, 244 / 255, 0.10)
                Column {
                    id: addInner
                    x: 10
                    y: 8
                    width: parent.width - 20
                    spacing: 8
                    Row {
                        spacing: 6
                        Text {
                            text: "+"
                            color: Theme.accent
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: panel.tr("settings.subscriptions.add", "Add subscription")
                            color: Theme.text
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    TextField {
                        id: subName
                        visible: subs.adding
                        width: parent.width
                        placeholderText: panel.tr("settings.subscriptions.name", "Name")
                        color: Theme.text
                        background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
                    }
                    TextField {
                        id: subAmt
                        visible: subs.adding
                        width: parent.width
                        placeholderText: panel.tr("settings.subscriptions.amount", "Amount")
                        color: Theme.text
                        background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
                    }
                    GlassButton {
                        visible: subs.adding
                        text: panel.tr("settings.subscriptions.save", "Save subscription")
                        onClicked: {
                            app.addSubscription(subName.text, Number(subAmt.text), "month")
                            subName.text = ""
                            subAmt.text = ""
                            subs.adding = false
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !subs.adding
                    cursorShape: Qt.PointingHandCursor
                    onClicked: subs.adding = true
                }
            }
        }
    }

    Component {
        id: syncComp
        SectionBody {
            HubCard {
                mode: "local"
                title: panel.tr("settings.sync.localOnly", "Local only")
                desc: panel.tr("settings.sync.localOnlyDesc", "This device only.")
            }
            Item { width: 1; height: 2 }
            HubCard {
                mode: "client"
                title: panel.tr("settings.sync.connectHub", "Connect to a hub")
                desc: panel.tr("settings.sync.connectHubDesc", "Send this device's usage to a hub elsewhere.")
            }
            Item { width: 1; height: 2 }
            HubCard {
                mode: "host"
                title: panel.tr("settings.sync.hostHub", "Host hub on this device")
                desc: panel.tr("settings.sync.hostHubDesc", "Open a hub here so other devices can connect.")
            }
            Item { width: 1; height: 6 }
            TextField {
                visible: (app.settings.hubMode || "local") === "client"
                width: parent.width
                placeholderText: panel.tr("settings.sync.hubUrl", "Hub URL")
                text: app.settings.hubUrl || ""
                color: Theme.text
                onEditingFinished: app.updateSetting("hubUrl", text)
                background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
            }
            SettingRow {
                hairline: false
                label: panel.tr("settings.sync.deviceId", "Device ID")
            }
            TextField {
                width: parent.width
                placeholderText: panel.tr("settings.sync.deviceIdPlaceholder", "auto (hostname)")
                text: app.settings.deviceId || ""
                color: Theme.text
                onEditingFinished: app.updateSetting("deviceId", text)
                background: Rectangle { radius: 8; color: Qt.rgba(1, 1, 1, 0.05); border.color: Theme.line; border.width: 1 }
            }
        }
    }
}
