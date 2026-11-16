import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One song, album, playlist, artist or station in a list. Inside an album
// (rowStyle "album") a song shows its track number instead of the cover and
// leaves out what the album header already says.
Rectangle {
    id: row

    required property int index
    required property string type
    required property string title
    required property string subtitle
    required property string album
    required property string artist
    required property string artwork
    required property string playId
    required property var durationMs
    required property bool explicit
    required property int trackNumber

    // "album", or "list" for everything else.
    property string rowStyle: "list"
    // The album's own artist, so a track only names a different one.
    property string albumArtist: ""

    readonly property bool isSong: type === "song"
    readonly property bool isCurrent: isSong && playId !== "" && playId === (player.current.playId || "")
    readonly property bool isPlayable: !isSong || playId !== ""
    readonly property bool isSelected: ListView.isCurrentItem && ListView.view.activeFocus
    readonly property bool inAlbum: rowStyle === "album" && isSong
    // Wide lists get the album in a column of its own.
    readonly property bool showAlbumColumn: !inAlbum && isSong && width > 760 && album !== ""
    readonly property color textColor: isSelected ? theme.selectionForeground : theme.foreground
    readonly property color mutedColor: isSelected ? theme.selectionForeground : theme.muted

    signal activated(int index)
    signal menuRequested(int index, real x, real y)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: inAlbum ? 44 : 60
    radius: Math.min(4, theme.radius)
    color: isSelected ? theme.selection : mouse.containsMouse ? theme.raised : "transparent"

    function formatDuration(ms) {
        const seconds = Math.round(ms / 1000)
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: row.isPlayable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: mouse => {
            row.ListView.view.currentIndex = row.index
            row.ListView.view.forceActiveFocus()
            if (mouse.button === Qt.RightButton) {
                const point = mapToItem(null, mouse.x, mouse.y)
                row.menuRequested(row.index, point.x, point.y)
            } else if (row.isPlayable) {
                row.activated(row.index)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 14
        spacing: 14

        // In an album: the track number, a play mark under the pointer, or
        // the accent while it plays.
        Item {
            visible: row.inAlbum
            Layout.preferredWidth: 28
            Layout.fillHeight: true
            Label {
                anchors.centerIn: parent
                visible: !mouse.containsMouse && !row.isCurrent
                text: row.trackNumber > 0 ? row.trackNumber : ""
                color: row.mutedColor
                font.pixelSize: 14
                font.features: { "tnum": 1 }
            }
            Glyph {
                anchors.centerIn: parent
                visible: (mouse.containsMouse && row.isPlayable) || row.isCurrent
                name: row.isCurrent && player.playing ? "volume" : "play"
                size: 16
                color: row.isCurrent ? theme.accent : row.textColor
            }
        }

        Rectangle {
            visible: !row.inAlbum
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: row.type === "artist" ? 22 : Math.min(4, theme.radius)
            color: theme.raised
            clip: true

            Glyph {
                anchors.centerIn: parent
                name: row.type === "station" ? "station" : "note"
                color: theme.muted
                visible: cover.status !== Image.Ready
            }
            RoundedImage {
                id: cover
                anchors.fill: parent
                radius: parent.radius
                source: !row.inAlbum && row.artwork !== "" ? music.artworkUrl(row.artwork, 88) : ""
                sourceSize: Qt.size(88, 88)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 3
            spacing: 2

            RowLayout {
                id: titleLine
                Layout.fillWidth: true
                spacing: 6
                // The title takes what it needs, so the E badge sits beside it.
                // The full title's width, as the most the title takes; an
                // eliding label's own implicit width shrinks as it elides.
                TextMetrics {
                    id: titleMetrics
                    font: titleLabel.font
                    text: row.title
                }
                Label {
                    id: titleLabel
                    Layout.fillWidth: true
                    Layout.maximumWidth: Math.ceil(titleMetrics.advanceWidth) + 2
                    text: row.title
                    elide: Text.ElideRight
                    font.pixelSize: row.inAlbum ? 14 : 15
                    font.weight: row.isCurrent ? Font.DemiBold : Font.Normal
                    color: row.isCurrent ? theme.accent : row.isPlayable ? row.textColor : theme.muted
                }
                KeyCap {
                    visible: row.explicit
                    key: "E"
                    tint: row.mutedColor
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                Layout.fillWidth: true
                // An album track only names its artist when it isn't the
                // album's (a feature, a compilation).
                text: row.inAlbum ? (row.artist !== row.albumArtist ? row.artist : "")
                    : row.isSong && row.album !== "" && !row.showAlbumColumn ? row.subtitle + " · " + row.album
                    : row.subtitle
                visible: text !== ""
                elide: Text.ElideRight
                color: row.mutedColor
                font.pixelSize: row.inAlbum ? 12 : 13
            }
        }

        Label {
            visible: row.showAlbumColumn
            Layout.fillWidth: true
            Layout.preferredWidth: 2
            text: row.album
            elide: Text.ElideRight
            color: row.mutedColor
            font.pixelSize: 13
        }

        Label {
            visible: row.isSong && row.durationMs > 0
            Layout.preferredWidth: 44
            horizontalAlignment: Text.AlignRight
            text: row.formatDuration(row.durationMs)
            color: row.mutedColor
            font.pixelSize: 13
            font.features: { "tnum": 1 }
        }
    }
}
