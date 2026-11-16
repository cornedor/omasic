import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtWebChannel
import QtWebEngine

// The hidden MusicKit page that plays the audio, and the Apple ID sign-in
// window it opens. Nothing here is visible except that sign-in dialog.
Item {
    id: root

    WebChannel { id: channel }

    WebEngineView {
        id: bridge
        width: 1
        height: 1
        visible: false
        profile: webProfile
        webChannel: channel

        // MusicKit's authorize() opens Apple's sign-in as a popup window.
        onNewWindowRequested: request => {
            signIn.open()
            request.openIn(signInView)
        }
        onJavaScriptConsoleMessage: (level, message, line, source) => {
            if (level >= WebEngineView.WarningMessageLevel)
                console.warn("musickit:", message)
        }
        // A crashed page would leave the app silent; start it again.
        onRenderProcessTerminated: (status, code) => root.load()
    }

    function load() {
        if (player.pageHtml !== "")
            bridge.loadHtml(player.pageHtml, player.pageBaseUrl)
    }

    Connections {
        target: player
        function onPageHtmlChanged() { root.load() }
    }

    Component.onCompleted: {
        channel.registerObject("player", player)
        load()
    }

    Popup {
        id: signIn
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 48)
        height: Math.min(760, parent.height - 48)
        modal: true
        padding: 0
        Material.roundedScale: theme.radius

        background: Rectangle { color: "white"; radius: theme.radius }

        contentItem: WebEngineView {
            id: signInView
            profile: webProfile
            onWindowCloseRequested: signIn.close()
        }

        onClosed: {
            signInView.url = "about:blank"
            // Closing the window by hand leaves MusicKit's promise hanging.
            if (player.signingIn)
                player.reportError("signIn", "cancelled")
        }
    }
}
