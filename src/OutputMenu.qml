import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Picks the device Omasic plays on.
Popup {
    id: menu

    padding: 8
    width: 340
    onAboutToShow: outputs.refresh()

    background: Rectangle {
        color: theme.raised
        radius: Math.min(6, theme.radius)
        border.color: theme.border
    }

    component Choice: Rectangle {
        id: choice
        property string text
        property string detail
        property bool chosen: false
        signal clicked()

        Layout.fillWidth: true
        implicitHeight: 44
        radius: Math.min(4, theme.radius)
        color: chosen ? theme.selection : choiceMouse.containsMouse ? theme.surface : "transparent"

        ColumnLayout {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: choice.text
                elide: Text.ElideRight
                font.pixelSize: 14
                font.weight: choice.chosen ? Font.DemiBold : Font.Normal
                color: choice.chosen ? theme.selectionForeground : theme.foreground
            }
            Label {
                Layout.fillWidth: true
                visible: choice.detail !== ""
                text: choice.detail
                elide: Text.ElideRight
                font.pixelSize: 11
                color: choice.chosen ? theme.selectionForeground : theme.muted
            }
        }
        MouseArea {
            id: choiceMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 2

        Label {
            text: qsTr("Play on")
            color: theme.muted
            font.pixelSize: 12
            font.bold: true
            leftPadding: 10
            topPadding: 4
            bottomPadding: 6
        }

        Choice {
            text: qsTr("System default")
            detail: {
                const device = outputs.devices.find(entry => entry.isDefault)
                return device ? device.description : ""
            }
            chosen: outputs.selected === ""
            onClicked: { outputs.select(""); menu.close() }
        }

        Repeater {
            model: outputs.devices
            delegate: Choice {
                required property var modelData
                text: modelData.description
                chosen: outputs.selected === modelData.name
                onClicked: { outputs.select(modelData.name); menu.close() }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: outputs.error !== ""
            text: outputs.error
            wrapMode: Text.Wrap
            color: theme.removed
            leftPadding: 10
            rightPadding: 10
        }
    }
}
