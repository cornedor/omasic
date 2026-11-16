import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Playback, visualizer, output, account and version.
FocusScope {
    id: root

    property bool visualizerControls: true
    signal visualizerControlsToggled()

    Component.onCompleted: outputs.refresh()

    // A titled group of rows.
    component Group: ColumnLayout {
        property alias title: heading.text
        default property alias rows: body.data
        Layout.fillWidth: true
        Layout.maximumWidth: 720
        spacing: 8

        Label {
            id: heading
            color: theme.muted
            font.pixelSize: 12
            font.bold: true
            topPadding: 18
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 8
            radius: Math.min(6, theme.radius)
            color: theme.surface
            border.color: theme.border
            ColumnLayout {
                id: body
                anchors.fill: parent
                anchors.margins: 4
                spacing: 0
            }
        }
    }

    // A setting: what it is, a line on what it does, and its control.
    component Row: Item {
        property alias label: name.text
        property alias detail: description.text
        default property alias control: slot.data
        Layout.fillWidth: true
        implicitHeight: Math.max(56, text.implicitHeight + 20)

        ColumnLayout {
            id: text
            anchors.left: parent.left
            anchors.right: slot.left
            anchors.leftMargin: 14
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Label {
                id: name
                Layout.fillWidth: true
                color: theme.foreground
                font.pixelSize: 14
                wrapMode: Text.Wrap
            }
            Label {
                id: description
                Layout.fillWidth: true
                visible: text !== ""
                color: theme.muted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
        Item {
            id: slot
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            width: implicitWidth
            height: implicitHeight
        }
    }

    // Two or more options side by side; the chosen one in the accent.
    component Choices: Row {
        id: choices
        property var options: []    // [{ value, label }]
        property var value
        signal chosen(var value)
        RowLayout {
            spacing: 6
            Repeater {
                model: choices.options
                delegate: ActionButton {
                    required property var modelData
                    small: true
                    text: modelData.label
                    primary: choices.value === modelData.value
                    onClicked: choices.chosen(modelData.value)
                }
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        WheelScroller { flickable: flick }

        ColumnLayout {
            id: column
            x: 24
            y: 24
            width: flick.width - 48
            spacing: 4

            Label {
                text: qsTr("Settings")
                color: theme.foreground
                font.pixelSize: 28
                font.bold: true
            }

            Group {
                title: qsTr("Playback")
                Row {
                    label: qsTr("Autoplay")
                    detail: qsTr("When the queue ends, keep playing similar music.")
                    ThemedSwitch {
                        checked: player.autoplay
                        onToggled: player.autoplay = !player.autoplay
                    }
                }
                Choices {
                    label: qsTr("Audio quality")
                    detail: qsTr("High streams at 256 kbps; Standard at 64 kbps uses less data.")
                    options: [{ value: 256, label: qsTr("High") }, { value: 64, label: qsTr("Standard") }]
                    value: player.bitrate
                    onChosen: value => player.bitrate = value
                }
            }

            Group {
                title: qsTr("Visualizer")
                Row {
                    label: qsTr("Controls")
                    detail: qsTr("Show favorite, previous, play and next when the pointer moves over the visualizer.")
                    ThemedSwitch {
                        checked: root.visualizerControls
                        onToggled: root.visualizerControlsToggled()
                    }
                }
            }

            Group {
                title: qsTr("Output")
                Repeater {
                    model: [{ name: "", description: qsTr("System default"),
                              detail: (outputs.devices.find(device => device.isDefault) || {}).description || "" }]
                           .concat(outputs.devices)
                    delegate: Row {
                        id: device
                        required property var modelData
                        label: modelData.description
                        detail: modelData.detail || ""
                        // A radio mark: the device Omasic plays on.
                        Rectangle {
                            width: 20
                            height: 20
                            radius: 10
                            color: "transparent"
                            border.width: 2
                            border.color: outputs.selected === device.modelData.name ? theme.accent : theme.border
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: theme.accent
                                visible: outputs.selected === device.modelData.name
                            }
                        }
                        MouseArea {
                            parent: device
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: outputs.select(device.modelData.name)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.margins: 14
                    visible: outputs.error !== ""
                    text: outputs.error
                    color: theme.removed
                    wrapMode: Text.Wrap
                }
            }

            Group {
                title: qsTr("Apple Music access")
                Row {
                    label: developerToken.isOfficial ? qsTr("Your MusicKit key") : qsTr("The web player's token")
                    detail: developerToken.isOfficial
                            ? qsTr("Team %1, key %2").arg(developerToken.teamId).arg(developerToken.keyId)
                            : qsTr("Not sanctioned by Apple for other apps; it can stop working at any time.")
                }
                KeyForm {
                    Layout.fillWidth: true
                    Layout.margins: 14
                }
                Row {
                    label: qsTr("Use the web player's token without a key")
                    detail: qsTr("A fallback for when you don't have an Apple Developer account.")
                    ThemedSwitch {
                        checked: developerToken.useWebToken
                        onToggled: developerToken.useWebToken = !developerToken.useWebToken
                    }
                }
            }

            Group {
                title: qsTr("Account")
                Row {
                    label: qsTr("Apple Music")
                    detail: music.storefront !== "" ? qsTr("Signed in, store country %1").arg(music.storefront.toUpperCase()) : ""
                    ActionButton {
                        small: true
                        text: qsTr("Sign out")
                        onClicked: player.signOut()
                    }
                }
            }

            Group {
                title: qsTr("About")
                Row {
                    label: "Omasic " + Qt.application.version
                    detail: qsTr("An Apple Music player for Omarchy. Not affiliated with or endorsed by Apple.")
                }
            }
        }
    }
}
