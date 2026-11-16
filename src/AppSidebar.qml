import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Search, Apple Music's own pages, and the library sections.
Rectangle {
    id: sidebar

    property string current: "home"
    readonly property alias searchField: search

    // In shortcut order: 2 is the first entry, 1 is search.
    // In shortcut order: 1 is search, then 2 onwards.
    readonly property var sections: [
        { key: "home", label: qsTr("Home"), group: "music", glyph: "home", shortcut: "2" },
        { key: "browse", label: qsTr("Browse"), group: "music", glyph: "browse", shortcut: "3" },
        { key: "recently-added", label: qsTr("Recently Added"), group: "library", shortcut: "4" },
        { key: "playlists", label: qsTr("Playlists"), group: "library", shortcut: "5" },
        { key: "albums", label: qsTr("Albums"), group: "library", shortcut: "6" },
        { key: "artists", label: qsTr("Artists"), group: "library", shortcut: "7" },
        { key: "songs", label: qsTr("Songs"), group: "library", shortcut: "8" }
    ]

    signal sectionRequested(string key)
    signal searchRequested(string term)
    signal settingsRequested()

    color: theme.surface
    implicitWidth: 230

    component SidebarEntry: Rectangle {
        id: entry
        property string text
        property string key
        property string glyph
        property bool selected: false
        signal clicked()

        Layout.fillWidth: true
        implicitHeight: 36
        radius: Math.min(4, theme.radius)
        color: selected ? theme.selection : entryMouse.containsMouse ? theme.raised : "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 8
            spacing: 10
            Glyph {
                visible: entry.glyph !== ""
                name: entry.glyph
                size: 18
                color: entry.selected ? theme.selectionForeground : theme.muted
            }
            Label {
                Layout.fillWidth: true
                text: entry.text
                elide: Text.ElideRight
                font.pixelSize: 14
                font.weight: entry.selected ? Font.DemiBold : Font.Normal
                color: entry.selected ? theme.selectionForeground : theme.foreground
            }
            KeyCap {
                visible: entry.key !== "" && (entryMouse.containsMouse || entry.selected)
                key: entry.key
                tint: entry.selected ? theme.selectionForeground : theme.muted
            }
        }
        MouseArea {
            id: entryMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: entry.clicked()
        }
    }

    component GroupLabel: Label {
        color: theme.muted
        font.pixelSize: 12
        font.bold: true
        leftPadding: 10
        topPadding: 14
        bottomPadding: 6
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 2

        // A Material TextField floats its placeholder as a label, which
        // collides with this flat field; the placeholder is drawn here instead.
        TextField {
            id: search
            Layout.fillWidth: true
            color: theme.foreground
            leftPadding: 34
            rightPadding: 12
            topPadding: 8
            bottomPadding: 8
            background: Rectangle {
                implicitHeight: 36
                radius: Math.min(4, theme.radius)
                color: theme.raised
                border.width: 1
                border.color: search.activeFocus ? theme.accent : theme.border
            }
            onAccepted: if (text.trim() !== "") sidebar.searchRequested(text)
            Keys.onEscapePressed: { text = ""; focus = false }

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                x: 10
                name: "search"
                size: 16
                color: theme.muted
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: search.leftPadding
                visible: search.text === "" && !search.preeditText
                text: qsTr("Search")
                color: theme.muted
                font: search.font
            }
        }

        GroupLabel { text: qsTr("Apple Music") }
        Repeater {
            model: sidebar.sections.filter(section => section.group === "music")
            delegate: SidebarEntry {
                required property var modelData
                text: modelData.label
                glyph: modelData.glyph
                key: modelData.shortcut
                selected: sidebar.current === modelData.key
                onClicked: sidebar.sectionRequested(modelData.key)
            }
        }

        GroupLabel { text: qsTr("Library") }
        Repeater {
            model: sidebar.sections.filter(section => section.group === "library")
            delegate: SidebarEntry {
                required property var modelData
                text: modelData.label
                key: modelData.shortcut
                selected: sidebar.current === modelData.key
                onClicked: sidebar.sectionRequested(modelData.key)
            }
        }

        Item { Layout.fillHeight: true }

        SidebarEntry {
            text: qsTr("Settings")
            glyph: "settings"
            key: "Ctrl+,"
            selected: sidebar.current === "settings"
            onClicked: sidebar.settingsRequested()
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: theme.border
    }
}
