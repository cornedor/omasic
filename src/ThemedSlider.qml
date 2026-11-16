import QtQuick
import QtQuick.Controls

// A thin slider in the theme's colors: a muted track, the accent up to the
// value, and a handle that grows under the pointer.
Slider {
    id: slider
    focusPolicy: Qt.NoFocus
    implicitHeight: 20

    background: Item {
        x: slider.leftPadding
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: 4

        Rectangle {
            anchors.fill: parent
            radius: Math.min(2, theme.radius)
            color: theme.border
        }
        Rectangle {
            width: slider.visualPosition * parent.width
            height: parent.height
            radius: Math.min(2, theme.radius)
            color: slider.enabled ? theme.accent : theme.muted
        }
    }

    handle: Rectangle {
        readonly property real diameter: slider.pressed || slider.hovered ? 14 : 10
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: diameter
        height: diameter
        radius: diameter / 2
        visible: slider.enabled
        color: theme.accent
    }
}
