import QtQuick

// An on/off switch in the theme's colors.
Rectangle {
    id: toggle

    property bool checked: false
    signal toggled()

    implicitWidth: 44
    implicitHeight: 24
    radius: height / 2
    color: checked ? theme.accent : theme.border
    opacity: enabled ? 1 : 0.4

    Rectangle {
        width: 18
        height: 18
        radius: 9
        anchors.verticalCenter: parent.verticalCenter
        x: toggle.checked ? parent.width - width - 3 : 3
        color: toggle.checked ? theme.accentForeground : theme.background
        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: toggle.toggled()
    }
}
