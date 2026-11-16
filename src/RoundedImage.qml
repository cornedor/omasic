import QtQuick
import QtQuick.Effects

// Artwork clipped to rounded corners, or a circle for artists. A Rectangle's
// radius doesn't clip what's inside it, so the image is masked instead.
Item {
    id: root

    property alias source: image.source
    property alias sourceSize: image.sourceSize
    property alias status: image.status
    property real radius: 0
    property color placeholderColor: theme.raised

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.placeholderColor
    }

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        // The mask only costs something when there are corners to round.
        layer.enabled: root.radius > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            // Smooth edges instead of a hard cut.
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Item {
        id: mask
        width: root.width
        height: root.height
        layer.enabled: true
        layer.smooth: true
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "black"
        }
    }
}
