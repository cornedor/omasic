import QtQuick
import QtQuick.Controls

// An album, playlist, station, artist or song as a card on a shelf.
Item {
    id: card

    required property var modelData
    required property int index
    readonly property bool isSelected: ListView.isCurrentItem && ListView.view.activeFocus
    readonly property bool isCurrent: modelData.type === "song" && modelData.playId === (player.current.playId || "")
                                      || modelData.type === "station" && modelData.id === (player.station.id || "")

    signal activated(int index)
    signal menuRequested(int index, real x, real y)

    width: 164
    height: 164 + 52

    Rectangle {
        id: cover
        width: 164
        height: 164
        radius: card.modelData.type === "artist" ? 82 : Math.min(6, theme.radius)
        color: theme.raised
        clip: true
        border.width: card.isSelected ? 3 : 0
        border.color: theme.accent

        Glyph {
            anchors.centerIn: parent
            name: card.modelData.type === "station" ? "station" : "note"
            size: 40
            color: theme.muted
            visible: art.status !== Image.Ready
        }
        RoundedImage {
            id: art
            anchors.fill: parent
            anchors.margins: card.isSelected ? 3 : 0
            radius: Math.max(0, cover.radius - anchors.margins)
            source: card.modelData.artwork ? music.artworkUrl(card.modelData.artwork, 328) : ""
            sourceSize: Qt.size(328, 328)
        }

        // A play badge on hover for what plays straight away.
        Rectangle {
            visible: mouse.containsMouse && (card.modelData.type === "song" || card.modelData.type === "station")
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            width: 36
            height: 36
            radius: 18
            color: theme.accent
            Glyph { anchors.centerIn: parent; name: "play"; size: 20; color: theme.accentForeground }
        }
    }

    Label {
        id: titleLabel
        anchors.top: cover.bottom
        anchors.topMargin: 8
        width: parent.width
        text: card.modelData.title
        elide: Text.ElideRight
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: card.isCurrent ? theme.accent : theme.foreground
    }
    Label {
        anchors.top: titleLabel.bottom
        anchors.topMargin: 2
        width: parent.width
        text: card.modelData.subtitle || ""
        elide: Text.ElideRight
        font.pixelSize: 12
        color: theme.muted
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => {
            card.ListView.view.currentIndex = card.index
            if (mouse.button === Qt.RightButton) {
                const point = mapToItem(null, mouse.x, mouse.y)
                card.menuRequested(card.index, point.x, point.y)
            } else {
                card.activated(card.index)
            }
        }
    }
}
