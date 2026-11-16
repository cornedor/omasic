import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What can be done with a song, album, playlist, artist or station: queue
// it, keep it, rate it, put it in a playlist, or go to where it's from.
Popup {
    id: menu

    property var item: ({})
    // Library, favorite and rating state, loaded when the menu opens.
    property var itemState: ({})
    // "actions", or "playlists" to pick one to add the song to.
    property string view: "actions"
    property var playlists: null

    signal openRequested(var item, string related)

    padding: 6
    width: 290
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: theme.raised
        radius: Math.min(6, theme.radius)
        border.color: theme.border
    }

    // Opens for an item at a point in the window, kept on screen.
    function openFor(target, x, y) {
        const normalized = Object.assign({}, target)
        // Queue entries only know the id they play by; for catalog songs
        // that's also their catalog id.
        if (!normalized.catalogId && normalized.type === "song" && normalized.playId && !normalized.playId.startsWith("i."))
            normalized.catalogId = normalized.playId
        item = normalized
        itemState = ({})
        view = "actions"
        music.loadState(normalized)
        parent = Overlay.overlay
        const height = Math.min(480, implicitHeight)
        menu.x = Math.max(8, Math.min(x, parent.width - width - 8))
        menu.y = Math.max(8, Math.min(y, parent.height - height - 8))
        open()
        actionList.currentIndex = 0
        actionList.forceActiveFocus()
    }

    Connections {
        target: music
        function onStateLoaded(id, loaded) {
            if (id === menu.item.id)
                menu.itemState = loaded
        }
    }

    readonly property bool isSong: item.type === "song"
    readonly property bool queueable: ["song", "album", "playlist"].includes(item.type) && (item.playId || item.id)
    readonly property bool hasCatalog: !!item.catalogId

    readonly property var actions: {
        const list = []
        if (queueable) {
            list.push({ label: qsTr("Play Next"), run: () => player.playNext(item) })
            list.push({ label: qsTr("Add to Queue"), run: () => player.addToQueue(item) })
        }
        if (hasCatalog && item.type !== "station" && item.type !== "artist") {
            if (itemState.inLibrary)
                list.push({ label: qsTr("In Your Library"), enabled: false })
            else
                list.push({ label: qsTr("Add to Library"), run: () => music.addToLibrary(item) })
        }
        if (hasCatalog && item.type !== "station") {
            list.push(itemState.inFavorites
                      ? { label: qsTr("Remove from Favorites"), run: () => music.setFavorite(item, false) }
                      : { label: qsTr("Favorite"), run: () => music.setFavorite(item, true) })
        }
        if (hasCatalog && (isSong || item.type === "album")) {
            list.push(itemState.rating === -1
                      ? { label: qsTr("Undo Suggest Less"), run: () => music.setSuggestLess(item, false) }
                      : { label: qsTr("Suggest Less"), run: () => music.setSuggestLess(item, true) })
        }
        if (isSong)
            list.push({ label: qsTr("Add to Playlist…"), keep: true, run: () => menu.showPlaylists() })
        if (isSong && (item.playId || item.id))
            list.push({ label: qsTr("Go to Album"), run: () => menu.openRequested(item, "album") })
        if ((isSong || item.type === "album") && (item.playId || item.catalogId || item.id))
            list.push({ label: qsTr("Go to Artist"), run: () => menu.openRequested(item, "artist") })
        if (!isSong && ["album", "playlist", "artist"].includes(item.type))
            list.push({ label: qsTr("Open"), run: () => menu.openRequested(item, "") })
        return list
    }

    function run(action) {
        if (!action || action.enabled === false)
            return
        if (!action.keep)
            close()
        action.run()
    }

    function showPlaylists() {
        if (!playlists)
            playlists = music.library("playlists")
        view = "playlists"
        newPlaylist.text = ""
        playlistList.currentIndex = -1
        newPlaylist.forceActiveFocus()
    }

    // One line in either view.
    component MenuLine: Rectangle {
        id: line
        property string text
        property bool selected: false
        property bool available: true
        signal clicked()

        width: ListView.view ? ListView.view.width : 0
        height: 34
        radius: Math.min(4, theme.radius)
        color: selected && available ? theme.selection : lineMouse.containsMouse && available ? theme.surface : "transparent"

        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            text: line.text
            elide: Text.ElideRight
            font.pixelSize: 14
            color: !line.available ? theme.muted : line.selected ? theme.selectionForeground : theme.foreground
        }
        MouseArea {
            id: lineMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: line.available ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (line.available) line.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 4

        // The item the menu is for.
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 4
            text: menu.view === "playlists" ? qsTr("Add “%1” to").arg(menu.item.title || "") : (menu.item.title || "")
            elide: Text.ElideRight
            font.pixelSize: 12
            font.bold: true
            color: theme.muted
        }

        ListView {
            id: actionList
            visible: menu.view === "actions"
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            interactive: false
            model: menu.actions
            keyNavigationEnabled: true
            delegate: MenuLine {
                required property var modelData
                required property int index
                text: modelData.label
                available: modelData.enabled !== false
                selected: ListView.isCurrentItem
                onClicked: menu.run(modelData)
            }
            Keys.onReturnPressed: menu.run(menu.actions[currentIndex])
            Keys.onEnterPressed: menu.run(menu.actions[currentIndex])
        }

        TextField {
            id: newPlaylist
            visible: menu.view === "playlists"
            Layout.fillWidth: true
            color: theme.foreground
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8
            background: Rectangle {
                implicitHeight: 34
                radius: Math.min(4, theme.radius)
                color: theme.surface
                border.width: 1
                border.color: newPlaylist.activeFocus ? theme.accent : theme.border
            }
            // Drawn here: a Material TextField floats its placeholder away.
            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: newPlaylist.leftPadding
                visible: newPlaylist.text === "" && !newPlaylist.preeditText
                text: qsTr("New playlist name, then Enter")
                color: theme.muted
                font: newPlaylist.font
            }
            onAccepted: {
                if (text.trim() === "")
                    return
                music.createPlaylist(text, menu.item)
                menu.close()
            }
            Keys.onDownPressed: { playlistList.currentIndex = 0; playlistList.forceActiveFocus() }
            Keys.onEscapePressed: menu.view = "actions"
        }

        ListView {
            id: playlistList
            visible: menu.view === "playlists"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(300, contentHeight)
            clip: true
            keyNavigationEnabled: true
            // Only playlists you can change; Apple's own mixes can't take songs.
            model: menu.playlists && menu.playlists.count >= 0
                   ? menu.playlists.itemsInSection("").filter(playlist => playlist.canEdit) : []
            delegate: MenuLine {
                required property var modelData
                required property int index
                text: modelData.title
                selected: ListView.isCurrentItem
                onClicked: { music.addToPlaylist(menu.item, modelData); menu.close() }
            }
            Keys.onReturnPressed: { music.addToPlaylist(menu.item, model[currentIndex]); menu.close() }
            Keys.onEnterPressed: { music.addToPlaylist(menu.item, model[currentIndex]); menu.close() }
            Keys.onUpPressed: currentIndex > 0 ? decrementCurrentIndex() : newPlaylist.forceActiveFocus()
            Keys.onEscapePressed: menu.view = "actions"
            WheelScroller { flickable: playlistList }

            BusyIndicator {
                anchors.centerIn: parent
                running: menu.playlists !== null && menu.playlists.loading && playlistList.count === 0
            }
        }
    }
}
