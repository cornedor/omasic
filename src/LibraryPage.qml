import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shows one Page from the backend: a header, then its items in sections.
FocusScope {
    id: root

    required property var page
    property bool canGoBack: false

    signal openRequested(var item)
    signal backRequested()
    // The item menu, at a point in the window.
    signal menuRequested(var item, real x, real y)

    // The menu for the selected row or card, beside it.
    function requestMenuForSelection() {
        if (shelves) {
            const row = shelfList.currentItem
            if (row && row.isSongList) {
                const songs = row.children.find(child => child.objectName === "songList")
                if (songs && songs.currentItem) {
                    const point = songs.currentItem.mapToItem(null, 120, songs.currentItem.height)
                    menuRequested(row.items[songs.currentIndex], point.x, point.y)
                }
                return
            }
            const shelf = row ? row.shelfView : null
            if (!shelf || shelf.currentIndex < 0 || !shelf.currentItem)
                return
            const point = shelf.currentItem.mapToItem(null, shelf.currentItem.width / 2, shelf.currentItem.height / 2)
            menuRequested(shelf.model[shelf.currentIndex], point.x, point.y)
        } else if (list.currentItem) {
            const point = list.currentItem.mapToItem(null, 120, list.currentItem.height)
            menuRequested(page.get(list.currentIndex), point.x, point.y)
        }
    }

    readonly property bool shelves: page.layout === "shelves"
    readonly property bool isAlbum: page.container.type === "album"
    readonly property bool isCollection: isAlbum || page.container.type === "playlist"
    readonly property bool isArtist: page.container.type === "artist"
    // Albums, playlists and artists get a large header.
    readonly property bool hasHero: isCollection || isArtist

    // Opens the artist (or album) an item is from.
    signal relatedRequested(var item, string related)
    // A section's full list, from "Show all".
    signal showAllRequested(string title, var items)

    // Song lists on shelf pages show this many rows before "Show all".
    readonly property int songListRows: 10

    function formatMinutes(ms) {
        return Math.round(ms / 60000)
    }

    // Plays a song (with the songs after it in `items`) or a station, and
    // opens anything else.
    function activateItem(item, items) {
        if (item.type === "song") {
            const songs = items.filter(entry => entry.type === "song" && entry.playId)
            const index = songs.findIndex(entry => entry.playId === item.playId)
            if (index >= 0)
                player.play(songs, index)
        } else if (item.type === "station") {
            player.playStation(item)
        } else if (item.type) {
            root.openRequested(item)
        }
    }

    function activate(row) {
        const item = page.get(row)
        if (item.type === "song") {
            const index = page.songIndex(row)
            if (index >= 0)
                player.play(page.songs(), index, page.container)
        } else {
            activateItem(item, [])
        }
    }

    // The page header scrolls with the list, like in Apple's apps: an album,
    // playlist or artist gets a large one, other pages a compact one.
    Component {
        id: pageHeader
        Item {
            width: ListView.view ? ListView.view.width : 0
            height: headerRow.implicitHeight + 24 + (root.hasHero ? 20 : 12)
            RowLayout {
                id: headerRow
                // The lists are inset (shelves 24, rows 12); the header sits
                // 24 from the page edge either way.
                x: root.shelves ? 0 : 12
                y: 24
                width: parent.width - x - 24
                spacing: 22

                IconButton {
                    visible: root.canGoBack
                    Layout.alignment: Qt.AlignTop
                    glyph: "back"
                    tip: qsTr("Back (Esc)")
                    onClicked: root.backRequested()
                }

                Rectangle {
                    visible: root.page.artwork !== ""
                    Layout.preferredWidth: root.hasHero ? 200 : 96
                    Layout.preferredHeight: root.hasHero ? 200 : 96
                    // An artist's photo is round, like everywhere in Apple Music.
                    radius: root.isArtist ? width / 2 : Math.min(6, theme.radius)
                    color: theme.raised
                    RoundedImage {
                        anchors.fill: parent
                        radius: parent.radius
                        source: root.page.artwork !== "" ? music.artworkUrl(root.page.artwork, 400) : ""
                        sourceSize: Qt.size(400, 400)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: root.hasHero ? Qt.AlignBottom : Qt.AlignVCenter
                    spacing: 4

                    Label {
                        visible: root.hasHero
                        text: root.isArtist ? qsTr("Artist") : root.isAlbum ? qsTr("Album") : qsTr("Playlist")
                        color: theme.muted
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.page.title
                        color: theme.foreground
                        font.pixelSize: root.hasHero ? 32 : 28
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    // An album's artist opens the artist; a playlist's curator is
                    // only a name.
                    Label {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: root.page.subtitle
                        color: root.isAlbum ? theme.accent : theme.muted
                        font.pixelSize: root.hasHero ? 17 : 15
                        font.underline: root.isAlbum && artistMouse.containsMouse
                        elide: Text.ElideRight
                        MouseArea {
                            id: artistMouse
                            anchors.fill: parent
                            enabled: root.isAlbum
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.relatedRequested(root.page.container, "artist")
                        }
                    }
                    Label {
                        visible: text !== ""
                        text: [root.page.container.genre, root.page.container.year].filter(part => part).join(" · ")
                        color: theme.muted
                        font.pixelSize: 13
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 640
                        visible: root.isCollection && text !== ""
                        text: root.page.container.description || ""
                        color: theme.muted
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    RowLayout {
                        visible: root.page.songCount > 0 && (!root.shelves || root.isArtist)
                        Layout.topMargin: root.hasHero ? 12 : 0
                        spacing: 8
                        ActionButton {
                            primary: true
                            text: qsTr("Play")
                            onClicked: player.play(root.page.songs(), 0, root.page.container)
                        }
                        ActionButton {
                            text: qsTr("Shuffle")
                            onClicked: {
                                const songs = root.page.songs()
                                player.shuffle = true
                                player.play(songs, Math.floor(Math.random() * songs.length), root.page.container)
                            }
                        }
                        IconButton {
                            id: moreButton
                            visible: root.hasHero
                            glyph: "more"
                            tip: qsTr("More")
                            onClicked: {
                                const point = moreButton.mapToItem(null, 0, moreButton.height + 4)
                                root.menuRequested(root.page.container, point.x, point.y)
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            visible: root.page.error !== ""
            text: root.page.error
            color: theme.removed
            wrapMode: Text.Wrap
        }

        // Home and Browse: a shelf of cards per section. Up and down move
        // between shelves, left and right along one.
        ListView {
            id: shelfList
            objectName: "shelfList"
            visible: root.shelves
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 24
            clip: true
            focus: root.shelves
            model: root.shelves ? root.page.sections : []
            header: pageHeader
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            spacing: 8
            maximumFlickVelocity: 8000
            ScrollBar.vertical: ScrollBar {}
            WheelScroller {
                flickable: shelfList
                horizontalAt: (x, y) => {
                    const row = shelfList.itemAt(x + shelfList.contentX, y + shelfList.contentY)
                    return row ? row.shelfView : null
                }
            }

            delegate: FocusScope {
                id: shelfScope
                required property string modelData
                required property int index
                width: shelfList.width
                height: shelfTitle.height + (isSongList ? songList.height : shelf.height) + 12
                focus: ListView.isCurrentItem
                // The shelf to scroll sideways; a song list has none.
                readonly property var shelfView: isSongList ? null : shelf
                readonly property var items: root.page.count >= 0 ? root.page.itemsInSection(modelData) : []
                // A section of only songs (an artist's top songs, a songs
                // chart) reads better as a list than as covers.
                readonly property bool isSongList: items.length > 0 && items.every(item => item.type === "song")

                // "Show all" beside a song list's title when there's more.
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.baseline: shelfTitle.baseline
                    visible: shelfScope.isSongList && shelfScope.items.length > root.songListRows
                    text: qsTr("Show all (%1)").arg(shelfScope.items.length)
                    color: theme.accent
                    font.pixelSize: 13
                    font.underline: showAllMouse.containsMouse
                    MouseArea {
                        id: showAllMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showAllRequested(shelfScope.modelData, shelfScope.items)
                    }
                }

                Label {
                    id: shelfTitle
                    text: shelfScope.modelData
                    color: theme.foreground
                    font.pixelSize: 19
                    font.bold: true
                    topPadding: 12
                    bottomPadding: 10
                }

                ListView {
                    id: songList
                    objectName: "songList"
                    visible: shelfScope.isSongList
                    anchors.top: shelfTitle.bottom
                    width: parent.width - 24
                    height: visible ? contentHeight : 0
                    interactive: false
                    focus: shelfScope.isSongList
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    model: shelfScope.isSongList ? shelfScope.items.slice(0, root.songListRows) : []
                    delegate: ItemRow {
                        required property var modelData
                        type: modelData.type
                        title: modelData.title
                        subtitle: modelData.subtitle
                        album: modelData.album || ""
                        artist: modelData.artist || ""
                        artwork: modelData.artwork || ""
                        playId: modelData.playId || ""
                        durationMs: modelData.durationMs || 0
                        explicit: !!modelData.explicit
                        trackNumber: modelData.trackNumber || 0
                        onActivated: index => root.activateItem(shelfScope.items[index], shelfScope.items)
                        onMenuRequested: (index, x, y) => root.menuRequested(shelfScope.items[index], x, y)
                    }
                    Keys.onReturnPressed: root.activateItem(shelfScope.items[currentIndex], shelfScope.items)
                    Keys.onEnterPressed: root.activateItem(shelfScope.items[currentIndex], shelfScope.items)
                }

                ListView {
                    id: shelf
                    visible: !shelfScope.isSongList
                    focus: !shelfScope.isSongList
                    anchors.top: shelfTitle.bottom
                    width: parent.width
                    height: 216
                    orientation: ListView.Horizontal
                    spacing: 18
                    // The page's WheelScroller scrolls shelves sideways, so
                    // the shelf itself leaves the wheel alone.
                    interactive: false
                    // Keep the selected card in view when the arrow keys move.
                    onCurrentIndexChanged: if (activeFocus) positionViewAtIndex(currentIndex, ListView.Contain)
                    clip: true
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    model: shelfScope.isSongList ? [] : shelfScope.items

                    delegate: ShelfCard {
                        onActivated: index => root.activateItem(shelf.model[index], shelf.model)
                        onMenuRequested: (index, x, y) => root.menuRequested(shelf.model[index], x, y)
                    }

                    Keys.onReturnPressed: root.activateItem(model[currentIndex], model)
                    Keys.onEnterPressed: root.activateItem(model[currentIndex], model)

                    NumberAnimation {
                        id: shelfScroll
                        target: shelf
                        property: "contentX"
                        duration: 260
                        easing.type: Easing.OutCubic
                    }

                    // Pages the shelf by most of its width.
                    function page(direction) {
                        const start = shelfScroll.running ? shelfScroll.to : contentX
                        const end = Math.max(originX, originX + contentWidth - width)
                        shelfScroll.stop()
                        shelfScroll.from = contentX
                        shelfScroll.to = Math.max(originX, Math.min(end, start + direction * (width - 120)))
                        shelfScroll.start()
                    }
                }

                HoverHandler { id: shelfHover }

                // Arrows over the covers when there's more to either side.
                component ShelfArrow: Rectangle {
                    id: arrow
                    property int direction: 1
                    width: 40
                    height: 40
                    radius: 20
                    y: shelf.y + 82 - height / 2
                    color: arrowMouse.containsMouse ? theme.raised : theme.surface
                    border.color: theme.border
                    opacity: 0.95
                    Glyph {
                        anchors.centerIn: parent
                        name: arrow.direction > 0 ? "next" : "previous"
                        size: 18
                    }
                    MouseArea {
                        id: arrowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shelf.page(arrow.direction)
                    }
                }
                ShelfArrow {
                    x: 8
                    direction: -1
                    visible: shelfHover.hovered && !shelf.atXBeginning
                }
                ShelfArrow {
                    x: parent.width - width - 24
                    direction: 1
                    visible: shelfHover.hovered && !shelf.atXEnd
                }

                MouseArea {
                    anchors.fill: shelfTitle
                    onClicked: shelfList.currentIndex = shelfScope.index
                }
            }
        }

        ListView {
            id: list
            visible: !root.shelves
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            clip: true
            focus: !root.shelves
            model: root.shelves ? null : root.page
            header: pageHeader
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            reuseItems: true
            maximumFlickVelocity: 8000
            ScrollBar.vertical: ScrollBar {}
            WheelScroller { flickable: list }

            // Albums group by disc when they have several; other pages by
            // their sections (search results, an artist's songs and albums).
            // A heading's height changing later doesn't move the rows below
            // it, so disc grouping only switches on once a second disc has
            // arrived; changing the property rebuilds the sections.
            section.property: root.isAlbum ? (root.page.discCount > 1 ? "discNumber" : "") : "section"
            section.delegate: Label {
                required property string section
                readonly property bool shown: section !== ""
                visible: shown
                height: shown ? implicitHeight : 0
                leftPadding: 10
                topPadding: 18
                bottomPadding: 6
                text: root.isAlbum ? qsTr("Disc %1").arg(section) : section
                color: theme.foreground
                font.pixelSize: 17
                font.bold: true
            }

            footer: Item {
                readonly property bool shown: root.isAlbum && root.page.songCount > 0
                width: list.width
                height: shown ? 64 : 0
                Label {
                    visible: parent.shown
                    x: 10
                    y: 16
                    text: qsTr("%n songs", "", root.page.songCount) + " · "
                          + qsTr("%n minutes", "", root.formatMinutes(root.page.totalDurationMs))
                    color: theme.muted
                    font.pixelSize: 13
                }
            }

            delegate: ItemRow {
                rowStyle: root.isAlbum ? "album" : "list"
                albumArtist: root.page.subtitle
                onActivated: index => root.activate(index)
                onMenuRequested: (index, x, y) => root.menuRequested(root.page.get(index), x, y)
            }

            Keys.onReturnPressed: root.activate(currentIndex)
            Keys.onEnterPressed: root.activate(currentIndex)

            Label {
                anchors.centerIn: parent
                visible: !root.page.loading && root.page.count === 0 && root.page.error === ""
                text: qsTr("Nothing here")
                color: theme.muted
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.page.loading && root.page.count === 0
    }
}
