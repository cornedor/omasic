import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1120
    height: 740
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    title: player.current.title ? player.current.title + " — Omasic" : "Omasic"
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    // Pages per sidebar section; F5 reloads the one shown.
    property var sectionPages: ({})
    readonly property bool typing: sidebar.searchField.activeFocus
    readonly property bool signedIn: player.ready && player.authorized && !developerToken.needsSetup
    property bool queueVisible: false
    property bool visualizerVisible: false
    Settings {
        id: visualizerSettings
        category: "visualizer"
        property bool controls: true
    }

    // The playing track as the music client knows items, and whether it's a
    // favorite, for the heart in the visualizer. Tracks that are only in
    // the library ("i." ids) have no catalog entry to favorite.
    readonly property string playingId: player.current.playId || ""
    readonly property var playingItem: {
        const item = Object.assign({}, player.current, { id: playingId })
        if (playingId !== "" && !playingId.startsWith("i."))
            item.catalogId = playingId
        return item
    }
    property var playingState: ({})
    function loadPlayingState() {
        playingState = ({})
        if (visualizerVisible && playingItem.catalogId)
            music.loadState(playingItem)
    }
    onPlayingIdChanged: loadPlayingState()
    onVisualizerVisibleChanged: loadPlayingState()
    function togglePlayingFavorite() {
        const favorite = !playingState.inFavorites
        playingState = Object.assign({}, playingState, { inFavorites: favorite })
        music.setFavorite(playingItem, favorite)
    }

    function showSection(key) {
        sidebar.current = key
        // Home and Browse change as you listen, and are quick to fetch, so
        // they load fresh on every visit; library sections stay for the
        // session (Songs can be thousands of rows) until F5 or a change.
        if (key === "home" || key === "browse")
            sectionPages[key] = key === "home" ? music.home() : music.browse()
        else if (!sectionPages[key])
            sectionPages[key] = music.library(key)
        stack.clear()
        stack.push(pageComponent, { page: sectionPages[key] })
    }

    function showSettings() {
        sidebar.current = "settings"
        stack.clear()
        stack.push(settingsComponent)
    }

    function showSearch(term) {
        sidebar.current = ""
        stack.clear()
        stack.push(pageComponent, { page: music.search(term) })
    }

    function open(item) {
        stack.push(pageComponent, { page: music.open(item), canGoBack: true })
    }

    // The album or artist ("album", "artist") of what's playing.
    function openFromTrack(type) {
        if (player.current.playId)
            openRelated(player.current, type)
    }

    // An item itself (related empty), or the album or artist it's from.
    function openRelated(item, related) {
        if (related === "")
            open(item)
        else
            stack.push(pageComponent, { page: music.openFromTrack(item, related), canGoBack: true })
    }

    function showMenu(item, x, y) {
        if (item && item.type)
            itemMenu.openFor(item, x, y)
    }

    // A short message after an action, like "Added … to your library".
    property string notice: ""
    property bool noticeOk: true
    Timer {
        id: noticeTimer
        interval: 3500
        onTriggered: win.notice = ""
    }
    Connections {
        target: music
        function onActionFinished(message, ok) {
            win.notice = message
            win.noticeOk = ok
            noticeTimer.restart()
            // The heart was turned before the answer came; a refusal turns
            // it back.
            if (!ok)
                win.loadPlayingState()
        }
        function onStateLoaded(id, loaded) {
            if (id !== "" && id === win.playingId)
                win.playingState = loaded
        }
        // Library pages load again on their next visit.
        function onLibraryChanged() {
            for (const key of ["recently-added", "playlists", "albums", "artists", "songs"])
                win.sectionPages[key] = null
        }
    }

    function back() {
        if (visualizerVisible)
            visualizerVisible = false
        else if (stack.depth > 1)
            stack.pop()
    }

    function reload() {
        if (stack.depth === 1 && sidebar.current !== "" && sidebar.current !== "settings") {
            sectionPages[sidebar.current] = null
            showSection(sidebar.current)
        }
    }

    function focusSearch() {
        sidebar.searchField.forceActiveFocus()
        sidebar.searchField.selectAll()
    }

    onSignedInChanged: {
        sectionPages = {}
        visualizerVisible = false
        if (signedIn)
            showSection("home")
        else
            stack.clear()
    }

    Connections {
        target: mpris
        function onRaiseRequested() { win.raise(); win.requestActivate() }
    }

    // Letter and Space shortcuts would swallow typing in the search field.
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.togglePlay() }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.next() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.previous() }
    // Plain arrows move through lists and shelves; with Shift they seek.
    Shortcut { sequence: "Shift+Right"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.seekBy(10) }
    Shortcut { sequence: "Shift+Left"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.seekBy(-10) }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.shuffle = !player.shuffle }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.cycleRepeat() }
    Shortcut { sequence: "U"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.signedIn; onActivated: win.queueVisible = !win.queueVisible }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.signedIn; onActivated: nowPlaying.toggleOutputs() }
    Shortcut { sequence: "V"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.signedIn; onActivated: win.visualizerVisible = !win.visualizerVisible }
    Shortcut { sequence: "G"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.visualizerVisible; onActivated: visualizer.glyphs = !visualizer.glyphs }
    Shortcut {
        sequences: ["M", "Menu", "Shift+F10"]
        context: Qt.ApplicationShortcut
        enabled: !win.typing && win.signedIn && !itemMenu.opened
        onActivated: if (stack.currentItem && stack.currentItem.requestMenuForSelection) stack.currentItem.requestMenuForSelection()
    }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.signedIn; onActivated: win.openFromTrack("album") }
    Shortcut { sequence: "Shift+A"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.signedIn; onActivated: win.openFromTrack("artist") }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.volume = player.volume + 0.05 }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: player.volume = player.volume - 0.05 }
    Shortcut { sequences: ["/", "Ctrl+F"]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.focusSearch() }
    Shortcut { sequences: ["Esc", "Alt+Left", "Back"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.back() }
    Shortcut { sequence: "Ctrl+,"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSettings() }
    Shortcut { sequences: ["F5", "Ctrl+R"]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.reload() }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: player.ready && !player.authorized && !player.signingIn && player.setupError === ""; onActivated: player.signIn() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.signedIn && !win.typing; onActivated: win.focusSearch() }
    Repeater {
        model: sidebar.sections
        delegate: Item {
            required property var modelData
            required property int index
            Shortcut {
                sequence: modelData.shortcut
                context: Qt.ApplicationShortcut
                enabled: win.signedIn && !win.typing
                onActivated: win.showSection(modelData.key)
            }
        }
    }

    WebPlayer { id: webPlayer }
    HelpOverlay { id: help }
    ItemMenu {
        id: itemMenu
        onOpenRequested: (item, related) => win.openRelated(item, related)
    }

    Component {
        id: settingsComponent
        SettingsPage {
            visualizerControls: visualizerSettings.controls
            onVisualizerControlsToggled: visualizerSettings.controls = !visualizerSettings.controls
        }
    }

    Component {
        id: pageComponent
        LibraryPage {
            onOpenRequested: item => win.open(item)
            onBackRequested: win.back()
            onMenuRequested: (item, x, y) => win.showMenu(item, x, y)
            onRelatedRequested: (item, related) => win.openRelated(item, related)
            onShowAllRequested: (title, items) => stack.push(pageComponent, { page: music.pageFromItems(title, items), canGoBack: true })
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            visible: win.signedIn

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                AppSidebar {
                    id: sidebar
                    Layout.fillHeight: true
                    onSectionRequested: key => win.showSection(key)
                    onSearchRequested: term => win.showSearch(term)
                    onSettingsRequested: win.showSettings()
                }

                StackView {
                    id: stack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: true
                    // Moving between pages is instant, like the rest of the app.
                    pushEnter: null
                    pushExit: null
                    popEnter: null
                    popExit: null
                    replaceEnter: null
                    replaceExit: null
                    onCurrentItemChanged: if (currentItem && !win.typing) currentItem.forceActiveFocus()
                }

                QueuePanel {
                    visible: win.queueVisible
                    Layout.fillHeight: true
                    onCloseRequested: win.queueVisible = false
                    onMenuRequested: (item, x, y) => win.showMenu(item, x, y)
                }
            }

            NowPlayingBar {
                id: nowPlaying
                Layout.fillWidth: true
                queueVisible: win.queueVisible
                onQueueToggled: win.queueVisible = !win.queueVisible
                onVisualizerToggled: win.visualizerVisible = !win.visualizerVisible
                onAlbumRequested: win.openFromTrack("album")
                onArtistRequested: win.openFromTrack("artist")
                onMenuRequested: (item, x, y) => win.showMenu(item, x, y)
            }
        }

        // Over the whole window, so nothing but the music is left to look at.
        Visualizer {
            id: visualizer
            anchors.fill: parent
            visible: opacity > 0
            opacity: win.visualizerVisible ? 1 : 0
            bass: levels.bass
            mid: levels.mid
            high: levels.high
            beat: levels.beat
            first: artworkPalette.first
            second: artworkPalette.second
            third: artworkPalette.third
            title: player.current.title || ""
            artist: player.current.artist || ""
            problem: levels.error
            playing: player.playing
            canPrevious: player.canPrevious
            canNext: player.canNext
            canFavorite: win.playingState.known === true
            favorite: win.playingState.inFavorites === true
            controlsEnabled: visualizerSettings.controls
            onCloseRequested: win.visualizerVisible = false
            onPreviousRequested: player.previous()
            onPlayToggled: player.togglePlay()
            onNextRequested: player.next()
            onFavoriteToggled: win.togglePlayingFavorite()

            Behavior on opacity { NumberAnimation { duration: 400 } }
        }
        // It listens and reads the artwork only while it shows.
        Binding { target: levels; property: "active"; value: win.visualizerVisible }
        Binding {
            target: artworkPalette
            property: "source"
            value: win.visualizerVisible && player.current.artwork ? music.artworkUrl(player.current.artwork, 64) : ""
        }
        Binding { target: artworkPalette; property: "fallback"; value: theme.accent }

        // No access to Apple Music set up yet: a MusicKit key, or the web
        // player's token as an opt-in.
        TokenSetup {
            anchors.fill: parent
            visible: !win.signedIn && developerToken.needsSetup
        }

        // Everything before the library: connecting, a setup problem, sign-in.
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(460, parent.width - 48)
            spacing: 18
            visible: !win.signedIn && !developerToken.needsSetup

            readonly property string problem: player.setupError !== "" ? player.setupError
                                              : developerToken.token === "" ? developerToken.error : ""

            Glyph {
                Layout.alignment: Qt.AlignHCenter
                name: "note"
                size: 56
                color: theme.accent
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Omasic"
                color: theme.foreground
                font.pixelSize: 30
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: parent.problem !== "" ? theme.removed : theme.muted
                text: parent.problem !== "" ? parent.problem
                      : !player.ready ? qsTr("Connecting to Apple Music…")
                      : player.signingIn ? qsTr("Signing in…")
                      : qsTr("Sign in with your Apple ID to play your Apple Music library.")
            }
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: parent.problem === "" && (!player.ready || player.signingIn)
                visible: running
            }
            ActionButton {
                Layout.alignment: Qt.AlignHCenter
                visible: parent.problem === "" && player.ready && !player.signingIn
                primary: true
                text: qsTr("Sign in")
                key: "Enter"
                onClicked: player.signIn()
            }
            ActionButton {
                Layout.alignment: Qt.AlignHCenter
                visible: developerToken.token === "" && developerToken.error !== "" && !developerToken.loading
                text: qsTr("Try again")
                onClicked: developerToken.refresh(true)
            }
            KeyForm {
                Layout.fillWidth: true
                visible: developerToken.isOfficial && developerToken.token === "" && developerToken.error !== ""
            }
        }

        // What an item menu action did; gone after a few seconds.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 100
            visible: win.notice !== "" && player.error === ""
            width: Math.min(noticeLabel.implicitWidth + 32, win.width - 48)
            height: noticeLabel.implicitHeight + 20
            radius: Math.min(6, theme.radius)
            color: theme.raised
            border.color: win.noticeOk ? theme.border : theme.removed
            Label {
                id: noticeLabel
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                text: win.notice
                wrapMode: Text.Wrap
                color: theme.foreground
            }
        }

        // Playback and sign-in errors, until dismissed or replaced.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: win.signedIn ? 100 : 24
            visible: player.error !== ""
            width: Math.min(toastRow.implicitWidth + 28, win.width - 48)
            height: toastRow.implicitHeight + 20
            radius: Math.min(6, theme.radius)
            color: theme.raised
            border.color: theme.removed

            RowLayout {
                id: toastRow
                anchors.centerIn: parent
                width: parent.width - 28
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    text: player.error
                    wrapMode: Text.Wrap
                    color: theme.foreground
                }
                ActionButton {
                    small: true
                    text: qsTr("Dismiss")
                    onClicked: player.clearError()
                }
            }
        }
    }
}
