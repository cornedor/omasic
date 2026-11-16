import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: help
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    padding: 28

    readonly property var shortcuts: [
        ["Space", qsTr("Play or pause")],
        ["N", qsTr("Next track")],
        ["P", qsTr("Previous track")],
        ["Shift ← →", qsTr("Back or forward 10 seconds")],
        ["S", qsTr("Shuffle on or off")],
        ["R", qsTr("Repeat off, all or one")],
        ["+ −", qsTr("Volume up or down")],
        ["U", qsTr("Show or hide the queue")],
        ["O", qsTr("Choose the output device")],
        ["A", qsTr("Go to the playing album")],
        ["Shift A", qsTr("Go to the playing artist")],
        ["/", qsTr("Search")],
        ["2–8", qsTr("Home, Browse and the library sections")],
        ["↑ ↓ ← →", qsTr("Move through lists and shelves")],
        ["Enter", qsTr("Play or open the selection")],
        ["M", qsTr("More for the selection: queue, library, favorite, playlist")],
        ["Esc", qsTr("Go back")],
        ["F5", qsTr("Reload the page")],
        ["Ctrl ,", qsTr("Settings")],
        ["?", qsTr("Show or hide this help")],
        ["Q", qsTr("Quit")]
    ]

    background: Rectangle {
        color: theme.raised
        radius: Math.min(6, theme.radius)
        border.color: theme.border
    }

    contentItem: ColumnLayout {
        spacing: 16

        Label {
            text: qsTr("Keyboard shortcuts")
            color: theme.foreground
            font.pixelSize: 20
            font.bold: true
        }

        GridLayout {
            columns: 2
            columnSpacing: 20
            rowSpacing: 10

            Repeater {
                model: help.shortcuts
                delegate: KeyCap {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 0
                    key: modelData[0]
                    tint: theme.accent
                }
            }
            Repeater {
                model: help.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 1
                    text: modelData[1]
                    color: theme.foreground
                }
            }
        }
    }
}
