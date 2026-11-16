import QtQuick
import QtQuick.Shapes

// A small icon drawn from a 24×24 path, so it stays sharp at any scale and
// takes any color.
Item {
    id: glyph

    property string name
    property color color: theme.foreground
    property real size: 20

    readonly property var paths: ({
        play: "M8 5v14l11-7z",
        pause: "M6 5h4v14H6zM14 5h4v14h-4z",
        next: "M6 6v12l8.5-6zM16 6h2v12h-2z",
        previous: "M18 6v12l-8.5-6zM6 6h2v12H6z",
        search: "M10 3a7 7 0 1 0 4.2 12.6l5.1 5.1 1.4-1.4-5.1-5.1A7 7 0 0 0 10 3zm0 2a5 5 0 1 1 0 10 5 5 0 0 1 0-10z",
        volume: "M4 9v6h4l5 4V5L8 9zM16 8.5a4.5 4.5 0 0 1 0 7v-2a2.5 2.5 0 0 0 0-3z",
        note: "M9 4v10.6A3.5 3.5 0 1 0 11 18V8h7V4z",
        back: "M20 11H7.8l5.6-5.6L12 4l-8 8 8 8 1.4-1.4L7.8 13H20z",
        shuffle: "M16 4v2h-2.6l-8 12H3v2h3.4l8-12H16v2l4-3zM3 6v2h2.4l2.2 3.3 1.2-1.8L6.4 6zM12.4 14.5l-1.2 1.8 2.2 3.7H16v2l4-3-4-3v2h-1.4z",
        repeat: "M7 7h10v3l4-4-4-4v3H5v6h2zM17 17H7v-3l-4 4 4 4v-3h12v-6h-2z",
        queue: "M3 5h13v2H3zM3 10h13v2H3zM3 15h8v2H3zM15 14v7l6-3.5z",
        output: "M6 2h12a1 1 0 0 1 1 1v18a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1zm6 3a2 2 0 1 0 0 4 2 2 0 0 0 0-4zm0 6a4.5 4.5 0 1 0 0 9 4.5 4.5 0 0 0 0-9zm0 2a2.5 2.5 0 1 1 0 5 2.5 2.5 0 0 1 0-5z",
        station: "M12 10a2 2 0 1 0 0 4 2 2 0 0 0 0-4zM7.8 7.8 6.4 6.4a8 8 0 0 0 0 11.2l1.4-1.4a6 6 0 0 1 0-8.4zm8.4 0a6 6 0 0 1 0 8.4l1.4 1.4a8 8 0 0 0 0-11.2z",
        home: "M12 3 2 12h3v9h5v-6h4v6h5v-9h3z",
        more: "M5 10a2 2 0 1 0 0 4 2 2 0 0 0 0-4zm7 0a2 2 0 1 0 0 4 2 2 0 0 0 0-4zm7 0a2 2 0 1 0 0 4 2 2 0 0 0 0-4z",
        autoplay: "M6.5 8a4 4 0 1 0 2.9 6.8L12 12.2l2.6 2.6A4 4 0 1 0 17.5 8a4 4 0 0 0-2.9 1.2L12 11.8 9.4 9.2A4 4 0 0 0 6.5 8zm0 2c.6 0 1.1.2 1.5.6L10.6 12 8 14.4a2 2 0 1 1-1.5-4.4zm11 0a2 2 0 1 1-1.5 3.4L13.4 12l2.6-2.4c.4-.4.9-.6 1.5-.6z",
        settings: "M9.78 4.73L10.18 1.66 13.82 1.66 14.22 4.73 15.57 5.29 18.02 3.40 20.60 5.98 18.71 8.43 19.27 9.78 22.34 10.18 22.34 13.82 19.27 14.22 18.71 15.57 20.60 18.02 18.02 20.60 15.57 18.71 14.22 19.27 13.82 22.34 10.18 22.34 9.78 19.27 8.43 18.71 5.98 20.60 3.40 18.02 5.29 15.57 4.73 14.22 1.66 13.82 1.66 10.18 4.73 9.78 5.29 8.43 3.40 5.98 5.98 3.40 8.43 5.29zM12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z",
        browse: "M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20zm0 2a8 8 0 1 1 0 16 8 8 0 0 1 0-16zm4.5 3.5-6 3-3 6 6-3z"
    })

    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: glyph.size / 24
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: glyph.color
            strokeWidth: -1
            strokeColor: "transparent"
            PathSvg { path: glyph.paths[glyph.name] || "" }
        }
    }
}
