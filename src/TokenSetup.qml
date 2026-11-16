import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// First run: how Omasic gets access to Apple Music. The supported way is the
// user's own MusicKit key; the web player's token is an opt-in fallback that
// says plainly what it is.
Flickable {
    id: setup

    contentWidth: width
    contentHeight: column.implicitHeight + 96
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        x: Math.max(24, (setup.width - width) / 2)
        y: 48
        width: Math.min(520, setup.width - 48)
        spacing: 14

        Glyph {
            Layout.alignment: Qt.AlignHCenter
            name: "note"
            size: 48
            color: theme.accent
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Connect Omasic to Apple Music")
            color: theme.foreground
            font.pixelSize: 24
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.muted
            text: qsTr("Omasic talks to Apple Music with a MusicKit key from your Apple Developer account. Create one at developer.apple.com under Certificates, Identifiers & Profiles, Keys, with MusicKit enabled, and download its .p8 file.")
        }

        KeyForm { Layout.fillWidth: true }

        Label {
            Layout.fillWidth: true
            visible: developerToken.error !== ""
            text: developerToken.error
            color: theme.removed
            wrapMode: Text.Wrap
        }
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: developerToken.loading
            visible: running
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            height: 1
            color: theme.border
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("No developer account?")
            color: theme.foreground
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.muted
            text: qsTr("Omasic can use the token Apple's own web player uses instead. Apple doesn't sanction this for other apps, and it can stop working at any time.")
        }
        ActionButton {
            text: qsTr("Use the web player's token")
            onClicked: developerToken.useWebToken = true
        }
    }
}
