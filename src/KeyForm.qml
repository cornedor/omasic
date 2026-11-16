import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The MusicKit key from an Apple Developer account: team id, key id and the
// .p8 file Apple gave to download once.
ColumnLayout {
    id: form

    spacing: 10

    component Field: ColumnLayout {
        property alias label: caption.text
        property alias text: input.text
        property alias placeholder: hint.text
        Layout.fillWidth: true
        spacing: 4
        Label {
            id: caption
            color: theme.foreground
            font.pixelSize: 13
        }
        TextField {
            id: input
            Layout.fillWidth: true
            color: theme.foreground
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8
            background: Rectangle {
                implicitHeight: 36
                radius: Math.min(4, theme.radius)
                color: theme.raised
                border.width: 1
                border.color: input.activeFocus ? theme.accent : theme.border
            }
            // Drawn here: a Material TextField floats its placeholder away.
            Label {
                id: hint
                anchors.verticalCenter: parent.verticalCenter
                x: input.leftPadding
                visible: input.text === "" && !input.preeditText
                color: theme.muted
                font: input.font
            }
            onAccepted: form.save()
        }
    }

    function save() {
        if (teamId.text.trim() !== "" && keyId.text.trim() !== "" && keyFile.text.trim() !== "")
            developerToken.setKey(teamId.text, keyId.text, keyFile.text)
    }

    Field {
        id: teamId
        label: qsTr("Team ID")
        placeholder: "ABCDE12345"
        text: developerToken.teamId
    }
    Field {
        id: keyId
        label: qsTr("Key ID")
        placeholder: "FGHIJ67890"
        text: developerToken.keyId
    }
    Field {
        id: keyFile
        label: qsTr("Key file")
        placeholder: "~/AuthKey_FGHIJ67890.p8"
        text: developerToken.keyFile
    }

    RowLayout {
        spacing: 8
        ActionButton {
            primary: true
            text: qsTr("Use this key")
            enabled: teamId.text.trim() !== "" && keyId.text.trim() !== "" && keyFile.text.trim() !== ""
            onClicked: form.save()
        }
        ActionButton {
            visible: developerToken.isOfficial
            text: qsTr("Remove the key")
            onClicked: developerToken.setKey("", "", "")
        }
    }
}
