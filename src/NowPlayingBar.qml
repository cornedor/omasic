import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The track that's playing, its progress, and the transport controls.
Rectangle {
    id: bar

    readonly property var track: player.current
    readonly property bool hasTrack: track.title !== undefined
    property bool queueVisible: false

    signal albumRequested()
    signal artistRequested()
    signal queueToggled()
    signal visualizerToggled()
    signal menuRequested(var item, real x, real y)

    function requestMenu(source, mouse) {
        const point = source.mapToItem(null, mouse.x, mouse.y)
        bar.menuRequested(bar.track, point.x, point.y)
    }

    implicitHeight: 84
    color: theme.surface

    function formatTime(seconds) {
        const total = Math.max(0, Math.floor(seconds))
        return Math.floor(total / 60) + ":" + String(total % 60).padStart(2, "0")
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 20
        spacing: 16

        // Artwork and title, a fixed share of the bar so the controls stay put.
        RowLayout {
            Layout.preferredWidth: bar.width * 0.28
            Layout.maximumWidth: bar.width * 0.28
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                radius: Math.min(4, theme.radius)
                color: theme.raised
                clip: true
                Glyph { anchors.centerIn: parent; name: "note"; color: theme.muted; visible: !art.visible }
                MouseArea {
                    id: coverMouse
                    anchors.fill: parent
                    enabled: bar.hasTrack
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => mouse.button === Qt.RightButton ? bar.requestMenu(coverMouse, mouse) : bar.albumRequested()
                }
                Image {
                    id: art
                    anchors.fill: parent
                    visible: status === Image.Ready
                    source: bar.hasTrack && bar.track.artwork ? music.artworkUrl(bar.track.artwork, 112) : ""
                    sourceSize: Qt.size(112, 112)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                // The title opens its album, the artist its artist page.
                Label {
                    Layout.fillWidth: true
                    text: bar.hasTrack ? bar.track.title : qsTr("Nothing playing")
                    color: bar.hasTrack ? theme.foreground : theme.muted
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    font.underline: titleMouse.containsMouse && bar.hasTrack
                    elide: Text.ElideRight
                    MouseArea {
                        id: titleMouse
                        anchors.fill: parent
                        enabled: bar.hasTrack
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mouse => mouse.button === Qt.RightButton ? bar.requestMenu(titleMouse, mouse) : bar.albumRequested()
                    }
                    ToolTip.visible: titleMouse.containsMouse && bar.track.album !== undefined && bar.track.album !== ""
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Go to %1 (A)").arg(bar.track.album || "")
                }
                Label {
                    Layout.fillWidth: true
                    visible: bar.hasTrack
                    text: bar.hasTrack ? bar.track.artist + (player.station.title ? " · " + player.station.title : "") : ""
                    color: theme.muted
                    font.pixelSize: 13
                    font.underline: artistMouse.containsMouse
                    elide: Text.ElideRight
                    MouseArea {
                        id: artistMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bar.artistRequested()
                    }
                    ToolTip.visible: artistMouse.containsMouse
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Go to %1 (Shift+A)").arg(bar.track.artist || "")
                }
            }
        }

        // Transport and progress.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                IconButton {
                    enabled: player.station.title === undefined
                    glyph: "shuffle"
                    checked: player.shuffle
                    tip: player.shuffle ? qsTr("Shuffle is on (S)") : qsTr("Shuffle is off (S)")
                    onClicked: player.shuffle = !player.shuffle
                }
                IconButton {
                    enabled: player.canPrevious
                    glyph: "previous"
                    tip: qsTr("Previous track (P)")
                    onClicked: player.previous()
                }

                // The main action: a circle in the accent, like a true circle
                // keeps its shape whatever the theme's rounding.
                Rectangle {
                    id: playButton
                    implicitWidth: 40
                    implicitHeight: 40
                    radius: 20
                    opacity: bar.hasTrack ? 1 : 0.4
                    color: playMouse.containsMouse && bar.hasTrack ? Qt.lighter(theme.accent, 1.08) : theme.accent

                    Glyph {
                        anchors.centerIn: parent
                        visible: !player.buffering
                        name: player.playing ? "pause" : "play"
                        size: 22
                        color: theme.accentForeground
                    }
                    BusyIndicator {
                        anchors.fill: parent
                        running: player.buffering
                        visible: running
                    }
                    MouseArea {
                        id: playMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (bar.hasTrack) player.togglePlay()
                    }
                    ToolTip.visible: playMouse.containsMouse && bar.hasTrack
                    ToolTip.delay: 500
                    ToolTip.text: player.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                }

                IconButton {
                    enabled: player.canNext
                    glyph: "next"
                    tip: qsTr("Next track (N)")
                    onClicked: player.next()
                }
                IconButton {
                    enabled: player.station.title === undefined
                    glyph: "repeat"
                    checked: player.repeat !== 0
                    tip: player.repeat === 1 ? qsTr("Repeating this track (R)")
                       : player.repeat === 2 ? qsTr("Repeating all (R)") : qsTr("Repeat is off (R)")
                    onClicked: player.cycleRepeat()

                    // Repeat one shows a small 1 on the icon.
                    Rectangle {
                        visible: player.repeat === 1
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        width: 12
                        height: 12
                        radius: 6
                        color: theme.accent
                        Label {
                            anchors.centerIn: parent
                            text: "1"
                            font.pixelSize: 9
                            font.bold: true
                            color: theme.accentForeground
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: bar.formatTime(progress.pressed ? progress.value : player.position)
                    color: theme.muted
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
                ThemedSlider {
                    id: progress
                    Layout.fillWidth: true
                    enabled: bar.hasTrack && player.duration > 0
                    from: 0
                    to: Math.max(1, player.duration)
                    value: player.position
                    onPressedChanged: if (!pressed) player.seek(value)
                }
                Label {
                    text: bar.formatTime(player.duration)
                    color: theme.muted
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
            }
        }

        // Visualizer, queue, output and volume.
        RowLayout {
            Layout.preferredWidth: 280
            Layout.maximumWidth: 280
            spacing: 6
            IconButton {
                glyph: "visualizer"
                tip: qsTr("Visualizer (V)")
                onClicked: bar.visualizerToggled()
            }
            IconButton {
                glyph: "queue"
                checked: bar.queueVisible
                tip: qsTr("Queue (U)")
                onClicked: bar.queueToggled()
            }
            IconButton {
                id: outputButton
                glyph: "output"
                checked: outputs.selected !== ""
                tip: qsTr("Output device (O)")
                onClicked: bar.toggleOutputs()
            }
            Glyph { Layout.leftMargin: 6; name: "volume"; color: theme.muted }
            ThemedSlider {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: player.volume
                onMoved: player.volume = value
            }
        }
    }

    function toggleOutputs() {
        if (outputMenu.opened) {
            outputMenu.close()
            return
        }
        // Opens upwards, ending at its button's right edge.
        outputMenu.x = outputButton.mapToItem(bar, 0, 0).x + outputButton.width - outputMenu.width
        outputMenu.open()
    }

    OutputMenu {
        id: outputMenu
        y: -implicitHeight - 8
    }
}
