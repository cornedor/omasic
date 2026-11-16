import QtQuick
import QtQuick.Controls

// The music as light: the artwork's colors flowing through a grid of
// characters, moved by the bass, the mids and the highs. Everything it
// shows comes in through properties, so it runs without the player too.
Item {
    id: root

    // 0 to 1 each; beat jumps to 1 on a beat and fades.
    property real bass: 0
    property real mid: 0
    property real high: 0
    property real beat: 0
    property color first: theme.accent
    property color second: theme.accent
    property color third: theme.accent
    // Characters, or the plain flowing light under them.
    property bool glyphs: true
    property string title: ""
    property string artist: ""
    property string problem: ""
    // The transport, for the controls that show while the pointer moves.
    property bool playing: false
    property bool canPrevious: false
    property bool canNext: false
    property bool canFavorite: false
    property bool favorite: false
    // Off leaves nothing but the picture; the keyboard still works.
    property bool controlsEnabled: true

    signal closeRequested()
    signal previousRequested()
    signal playToggled()
    signal nextRequested()
    signal favoriteToggled()

    // Dimmest to brightest; the shader picks by index.
    readonly property string ramp: " ·.:-=+*xoX#%@"
    readonly property int cellWidth: 10
    readonly property int cellHeight: 20
    readonly property real columns: width / cellWidth
    readonly property real rows: height / cellHeight

    property real flow: 0
    property real drift: 0
    property real time: 0
    property real beatAge: 10

    onBeatChanged: if (beat >= 1) beatAge = 0

    // A new track's colors wash in rather than snap.
    Behavior on first { ColorAnimation { duration: 2500 } }
    Behavior on second { ColorAnimation { duration: 2500 } }
    Behavior on third { ColorAnimation { duration: 2500 } }

    FrameAnimation {
        running: root.visible
        onTriggered: {
            // Loud music lives faster; silence still breathes.
            root.flow += frameTime * (0.7 + 1.3 * root.bass + 0.6 * root.mid)
            root.drift += frameTime
            root.time += frameTime
            root.beatAge += frameTime
        }
    }

    // The characters, drawn once into a strip the shader reads from.
    Row {
        id: atlasRow
        Repeater {
            model: root.ramp.length
            delegate: Text {
                required property int index
                width: root.cellWidth
                height: root.cellHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.ramp.charAt(index)
                color: "white"
                font.family: "monospace"
                font.pixelSize: 16
                font.bold: true
            }
        }
    }
    ShaderEffectSource {
        id: atlasSource
        sourceItem: atlasRow
        hideSource: true
        smooth: true
        textureSize: Qt.size(atlasRow.width * Screen.devicePixelRatio, atlasRow.height * Screen.devicePixelRatio)
    }

    // The color field, in square texels: two across a cell, four down.
    ShaderEffect {
        id: fieldEffect
        width: Math.max(2, Math.ceil(root.columns) * 2)
        height: Math.max(2, Math.ceil(root.rows) * 4)
        blending: false
        fragmentShader: "qrc:/field.frag.qsb"

        property real flow: root.flow
        property real drift: root.drift
        property real aspect: root.width / Math.max(1, root.height)
        property real bass: root.bass
        property real mid: root.mid
        property real beatAge: root.beatAge
        property color first: root.first
        property color second: root.second
        property color third: root.third
    }
    ShaderEffectSource {
        id: fieldSource
        sourceItem: fieldEffect
        hideSource: true
        smooth: true
        textureSize: Qt.size(fieldEffect.width, fieldEffect.height)
    }

    ShaderEffect {
        anchors.fill: parent
        blending: false
        fragmentShader: "qrc:/glyphs.frag.qsb"

        property var field: fieldSource
        property var atlas: atlasSource
        property size cells: Qt.size(root.columns, root.rows)
        property real glyphCount: root.ramp.length
        property real glyphMix: root.glyphs ? 1 : 0
        property real high: root.high
        property real time: root.time
        property color backdrop: "#06060a"

        Behavior on glyphMix { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
    }

    // Clicks stay off the page underneath. Moving or clicking brings up the
    // controls; once the pointer rests, they and the pointer get out of
    // the way again.
    readonly property bool pointerShown: pointerRest.running || controlsHover.hovered
    readonly property bool controlsShown: controlsEnabled && pointerShown
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        cursorShape: root.pointerShown ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: pointerRest.restart()
        onPressed: pointerRest.restart()
        onDoubleClicked: root.closeRequested()
        onWheel: wheel => wheel.accepted = true
    }
    Timer {
        id: pointerRest
        interval: 2500
    }

    // Always light on dark, whatever the theme: it sits on the picture.
    component ControlButton: Rectangle {
        id: control
        property string glyph
        property string tip
        property real size: 40
        property real glyphSize: 20
        property color tint: "white"
        signal clicked()

        implicitWidth: size
        implicitHeight: size
        radius: Math.min(size / 2, theme.radius)
        opacity: enabled ? 1 : 0.35
        color: controlMouse.containsMouse && enabled ? Qt.alpha("white", 0.16) : "transparent"

        Glyph {
            anchors.centerIn: parent
            name: control.glyph
            size: control.glyphSize
            color: control.tint
        }
        MouseArea {
            id: controlMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: if (control.enabled) control.clicked()
        }
        ToolTip.visible: controlMouse.containsMouse && tip.length > 0
        ToolTip.delay: 500
        ToolTip.text: tip
    }

    Rectangle {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: controlRow.implicitWidth + 24
        height: controlRow.implicitHeight + 16
        radius: Math.min(height / 2, theme.radius)
        color: Qt.alpha("#06060a", 0.72)
        border.color: Qt.alpha("white", 0.14)
        opacity: root.controlsShown ? 1 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 350 } }
        HoverHandler { id: controlsHover }

        Row {
            id: controlRow
            anchors.centerIn: parent
            spacing: 6

            ControlButton {
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.canFavorite
                glyph: root.favorite ? "heartFilled" : "heart"
                tint: root.favorite ? theme.accent : "white"
                tip: root.favorite ? qsTr("Remove from Favorites") : qsTr("Favorite")
                onClicked: root.favoriteToggled()
            }
            Item { width: 10; height: 1 }
            ControlButton {
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.canPrevious
                glyph: "previous"
                tip: qsTr("Previous track (P)")
                onClicked: root.previousRequested()
            }
            ControlButton {
                anchors.verticalCenter: parent.verticalCenter
                size: 48
                glyphSize: 28
                glyph: root.playing ? "pause" : "play"
                tip: root.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                onClicked: root.playToggled()
            }
            ControlButton {
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.canNext
                glyph: "next"
                tip: qsTr("Next track (N)")
                onClicked: root.nextRequested()
            }
            Item { width: 10; height: 1 }
            ControlButton {
                anchors.verticalCenter: parent.verticalCenter
                glyph: "close"
                tip: qsTr("Close the visualizer (Esc)")
                onClicked: root.closeRequested()
            }
        }
    }

    // What's playing: for a moment after each track starts, and with the
    // controls.
    Column {
        id: caption
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 32
        width: Math.max(120, (parent.width - controls.width) / 2 - 56)
        spacing: 4
        property real announced: 0
        opacity: Math.max(announced, controls.opacity)

        Label {
            width: parent.width
            text: root.title
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: 22
            font.bold: true
        }
        Label {
            width: parent.width
            text: root.artist
            elide: Text.ElideRight
            color: Qt.alpha("white", 0.7)
            font.pixelSize: 15
        }

        SequentialAnimation {
            id: captionShow
            NumberAnimation { target: caption; property: "announced"; to: 1; duration: 600 }
            PauseAnimation { duration: 5000 }
            NumberAnimation { target: caption; property: "announced"; to: 0; duration: 1800 }
        }
    }
    onTitleChanged: if (visible && title !== "") captionShow.restart()
    onVisibleChanged: if (visible && title !== "") captionShow.restart()

    // Why it isn't moving to the music, when it can't hear it.
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 24
        width: Math.min(implicitWidth, parent.width - 48)
        visible: root.problem !== ""
        text: root.problem
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: Qt.alpha("white", 0.8)
    }
}
