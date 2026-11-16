import QtQuick

// Scrolls Flickables the way GTK apps on the same desktop do. Wayland only
// reports finger and wheel movement; how far content moves and how it coasts
// afterwards is up to each toolkit, and Qt's own handling (an animation per
// wheel notch, constant-rate flicks) feels slow next to them. The numbers
// follow GTK 4's GtkScrolledWindow and GtkEventControllerScroll.
//
// It scrolls `flickable` vertically, and sideways whatever `horizontalAt`
// returns for the pointer's position (a shelf in a list of shelves), so one
// handler decides for both directions.
WheelHandler {
    id: scroller

    required property Flickable flickable
    // (x, y in flickable coordinates) => the Flickable to scroll sideways,
    // or null.
    property var horizontalAt: null
    // GTK scales touchpad movement by this (MAGIC_SCROLL_FACTOR).
    property real touchpadFactor: 2.5
    // Coasting slows as exp(-friction * t) (DECELERATION_FRICTION).
    property real friction: 4

    // One per direction: what it scrolls, recent finger movement
    // ([{time, delta}] over the last 150 ms), and its coasting speed.
    property var axes: ({
        x: { horizontal: true, target: null, samples: [], velocity: 0, lastEvent: 0 },
        y: { horizontal: false, target: null, samples: [], velocity: 0, lastEvent: 0 }
    })

    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

    function scrollBy(f, horizontal, delta) {
        if (!f)
            return false
        if (horizontal) {
            const start = f.originX - f.leftMargin
            const end = Math.max(start, f.originX + f.contentWidth + f.rightMargin - f.width)
            const x = Math.max(start, Math.min(end, f.contentX + delta))
            const moved = x !== f.contentX
            f.contentX = x
            return moved
        }
        const top = f.originY - f.topMargin
        const bottom = Math.max(top, f.originY + f.contentHeight + f.bottomMargin - f.height)
        const y = Math.max(top, Math.min(bottom, f.contentY + delta))
        const moved = y !== f.contentY
        f.contentY = y
        return moved
    }

    // GTK's step per wheel notch: the visible length to the power 2/3.
    function notchStep(f, horizontal) {
        return Math.pow(horizontal ? f.width : f.height, 2 / 3)
    }

    function stopCoasting() {
        axes.x.velocity = 0
        axes.y.velocity = 0
        coast.running = false
    }

    // GTK's release speed: everything within 150 ms of the last movement,
    // over the time it spans. If the fingers rested before lifting, there's
    // nothing to coast on.
    function releaseVelocity(axis, now) {
        const samples = axis.samples
        if (samples.length < 2 || now - samples[samples.length - 1].time > 150)
            return 0
        const last = samples[samples.length - 1].time
        const recent = samples.filter(sample => last - sample.time <= 150)
        const span = last - recent[0].time
        if (span <= 0)
            return 0
        return recent.reduce((sum, sample) => sum + sample.delta, 0) / span * 1000
    }

    // A swipe in the direction it's already coasting adds to the speed, as
    // in GTK, so a few quick swipes cover a long list.
    function accumulate(next, current) {
        if (current === 0 || (next >= 0) !== (current >= 0) || Math.abs(next) < Math.abs(current) * 0.33)
            return next
        const multiplier = (next - current * 0.33) / (current - current * 0.33)
        return next + current * Math.min(multiplier, 6)
    }

    function track(axis, delta, now) {
        if (now - axis.lastEvent > 150)
            axis.samples = []
        axis.lastEvent = now
        if (delta === 0)
            return
        axis.samples = axis.samples.filter(sample => now - sample.time <= 150).concat([{ time: now, delta: delta }])
        scrollBy(axis.target, axis.horizontal, delta)
    }

    // A WheelHandler only takes events in its orientation, and this one is
    // vertical; purely sideways movement comes through this twin, which
    // runs the same code. Whichever takes an event handles both directions.
    readonly property WheelHandler sideways: WheelHandler {
        parent: scroller.parent
        target: null
        orientation: Qt.Horizontal
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => scroller.handle(event, point.position)
    }

    onWheel: event => handle(event, point.position)

    function handle(event, pointerPosition) {
        const now = Date.now()
        const position = scroller.parent.mapToItem(flickable, pointerPosition.x, pointerPosition.y)
        const side = horizontalAt ? horizontalAt(position.x, position.y) : null

        // A touchpad scrolls in phases; a mouse wheel's notches have none,
        // even when Wayland also gives them a pixel distance.
        if (event.phase === Qt.NoScrollPhase) {
            stopCoasting()
            flickable.cancelFlick()
            // Shift turns the wheel sideways, as everywhere else.
            const sideways = (event.modifiers & Qt.ShiftModifier) ? event.angleDelta.y : event.angleDelta.x
            const down = (event.modifiers & Qt.ShiftModifier) ? 0 : event.angleDelta.y
            if (side && sideways !== 0)
                scrollBy(side, true, -sideways / 120 * notchStep(side, true))
            if (down !== 0)
                scrollBy(flickable, false, -down / 120 * notchStep(flickable, false))
            return
        }

        if (event.phase === Qt.ScrollEnd) {
            for (const axis of [axes.x, axes.y]) {
                // velocity still holds the coasting speed the fingers caught.
                axis.velocity = accumulate(releaseVelocity(axis, now), axis.velocity)
                axis.samples = []
            }
            coast.running = Math.abs(axes.x.velocity) > 60 || Math.abs(axes.y.velocity) > 60
            return
        }

        // Fingers on the pad: hold the content, keep the speed for when they
        // lift, and pick what scrolls sideways at the start of a gesture.
        coast.running = false
        if (now - axes.x.lastEvent > 150 || !axes.x.target)
            axes.x.target = side
        axes.y.target = flickable
        track(axes.x, -event.pixelDelta.x * touchpadFactor, now)
        track(axes.y, -event.pixelDelta.y * touchpadFactor, now)
    }

    readonly property FrameAnimation coast: FrameAnimation {
        onTriggered: {
            const dt = frameTime
            let moving = false
            for (const axis of [scroller.axes.x, scroller.axes.y]) {
                if (axis.velocity === 0)
                    continue
                if (!scroller.scrollBy(axis.target, axis.horizontal, axis.velocity * dt))
                    axis.velocity = 0
                axis.velocity *= Math.exp(-scroller.friction * dt)
                if (Math.abs(axis.velocity) < 20)
                    axis.velocity = 0
                moving = moving || axis.velocity !== 0
            }
            if (!moving)
                running = false
        }
    }
}
