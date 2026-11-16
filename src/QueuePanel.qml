import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What's playing and what comes next, as MusicKit has it (shuffled or not).
Rectangle {
    id: panel

    signal closeRequested()
    signal menuRequested(var item, real x, real y)

    color: theme.surface
    implicitWidth: 320

    function formatDuration(ms) {
        const seconds = Math.round(ms / 1000)
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: player.station.title ? player.station.title : qsTr("Up Next")
                elide: Text.ElideRight
                color: theme.foreground
                font.pixelSize: 17
                font.bold: true
            }
            IconButton {
                glyph: "autoplay"
                checked: player.autoplay
                tip: player.autoplay ? qsTr("Autoplay is on: similar music plays when the queue ends")
                                     : qsTr("Autoplay is off: playback stops when the queue ends")
                onClicked: player.autoplay = !player.autoplay
            }
            IconButton {
                glyph: "queue"
                checked: true
                tip: qsTr("Hide the queue (U)")
                onClicked: panel.closeRequested()
            }
        }

        ListView {
            id: queueList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: player.queue
            currentIndex: player.index
            highlightMoveDuration: 0
            maximumFlickVelocity: 8000
            ScrollBar.vertical: ScrollBar {}
            WheelScroller { flickable: queueList }

            // Keep what's playing in view as tracks change.
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                id: entry
                required property var modelData
                required property int index
                readonly property bool isCurrent: index === player.index
                readonly property bool isPast: index < player.index
                // Autoplay's songs start under their own heading.
                readonly property bool startsAutoplay: !!modelData.isAutoplay
                                                      && (index === 0 || !player.queue[index - 1].isAutoplay)

                width: queueList.width
                height: 52 + (startsAutoplay ? autoplayHeading.height : 0)
                color: "transparent"
                opacity: isPast ? 0.55 : 1

                // The highlight covers the song, not the heading above it.
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 52
                    radius: Math.min(4, theme.radius)
                    color: entry.isCurrent ? theme.selection : entryMouse.containsMouse ? theme.raised : "transparent"
                }

                Label {
                    id: autoplayHeading
                    visible: entry.startsAutoplay
                    text: qsTr("Autoplay")
                    color: theme.muted
                    font.pixelSize: 12
                    font.bold: true
                    leftPadding: 8
                    topPadding: 12
                    bottomPadding: 6
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 52
                    anchors.leftMargin: 8
                    anchors.rightMargin: 10
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Math.min(4, theme.radius)
                        color: theme.raised
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: entry.modelData.artwork ? music.artworkUrl(entry.modelData.artwork, 72) : ""
                            sourceSize: Qt.size(72, 72)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            Layout.fillWidth: true
                            text: entry.modelData.title || ""
                            elide: Text.ElideRight
                            font.pixelSize: 13
                            font.weight: entry.isCurrent ? Font.DemiBold : Font.Normal
                            color: entry.isCurrent ? theme.selectionForeground : theme.foreground
                        }
                        Label {
                            Layout.fillWidth: true
                            // MusicKit fills the artist in once the track loads.
                            text: entry.modelData.artist || ""
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            color: entry.isCurrent ? theme.selectionForeground : theme.muted
                        }
                    }
                    Label {
                        visible: entry.modelData.durationMs > 0
                        text: panel.formatDuration(entry.modelData.durationMs)
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                        color: entry.isCurrent ? theme.selectionForeground : theme.muted
                    }
                }

                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) {
                            const point = mapToItem(null, mouse.x, mouse.y)
                            panel.menuRequested(entry.modelData, point.x, point.y)
                        } else {
                            player.jump(entry.index)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: queueList.count === 0
                text: qsTr("Nothing queued")
                color: theme.muted
            }
        }
    }
}
