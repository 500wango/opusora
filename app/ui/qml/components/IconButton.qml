import QtQuick
import QtQuick.Controls
import Opusora

Button {
    id: root

    property string iconName
    property bool emphasized: false
    property bool active: false
    property bool quiet: false
    property string tooltipText: ""
    readonly property color iconColor: !enabled
        ? Theme.textSecondary
        : emphasized
            ? Theme.playerPrimaryIcon
            : active
                ? Theme.accent
                : Theme.textPrimary

    implicitWidth: emphasized ? 42 : 34
    implicitHeight: emphasized ? 42 : 34
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.text: tooltipText

    background: Rectangle {
        radius: root.emphasized ? width / 2 : 10
        color: !root.enabled
            ? (root.quiet ? "transparent" : Theme.controlFill)
            : root.emphasized
                ? (root.down ? Theme.playerPrimaryControlPressed : Theme.playerPrimaryControl)
                : root.active
                    ? Theme.accentSoft
                    : root.quiet
                        ? (root.down ? Theme.controlPressed : (root.hovered ? Theme.controlHover : "transparent"))
                        : (root.down ? Theme.controlPressed : (root.hovered ? Theme.controlHover : Theme.controlFill))
        border.color: root.emphasized
            ? Qt.rgba(1, 1, 1, Theme.mode === "dark" ? 0.42 : 0.18)
            : (root.quiet && !root.active ? "transparent" : Theme.playerControlStroke)
        border.width: 1
        opacity: root.enabled ? 1.0 : 0.50

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height / 2
            radius: parent.radius
            color: root.emphasized ? Qt.rgba(1, 1, 1, 0.24) : Theme.glassHighlight
            opacity: root.quiet && !root.emphasized ? 0.0 : (root.down ? 0.10 : 0.24)
        }

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    contentItem: Item {
        Canvas {
            id: glyph
            anchors.centerIn: parent
            width: root.emphasized ? 20 : 17
            height: width
            opacity: root.enabled ? 1.0 : 0.60
            property string watchedIconName: root.iconName
            property color watchedIconColor: root.iconColor
            property bool watchedHovered: root.hovered
            property bool watchedDown: root.down
            property bool watchedEnabled: root.enabled
            property bool watchedEmphasized: root.emphasized

            onWatchedIconNameChanged: requestPaint()
            onWatchedIconColorChanged: requestPaint()
            onWatchedHoveredChanged: requestPaint()
            onWatchedDownChanged: requestPaint()
            onWatchedEnabledChanged: requestPaint()
            onWatchedEmphasizedChanged: requestPaint()

            onPaint: {
                const ctx = getContext("2d")
                const w = width
                const h = height
                const c = root.iconColor
                ctx.clearRect(0, 0, w, h)
                ctx.strokeStyle = c
                ctx.fillStyle = c
                ctx.lineWidth = root.emphasized ? 2.15 : 1.75
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                function line(x1, y1, x2, y2) {
                    ctx.beginPath()
                    ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    ctx.stroke()
                }

                function triangle(x1, y1, x2, y2, x3, y3, fillShape) {
                    ctx.beginPath()
                    ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    ctx.lineTo(x3, y3)
                    ctx.closePath()
                    if (fillShape) {
                        ctx.fill()
                    } else {
                        ctx.stroke()
                    }
                }

                function chevron(up) {
                    ctx.beginPath()
                    if (up) {
                        ctx.moveTo(w * 0.24, h * 0.62)
                        ctx.lineTo(w * 0.50, h * 0.36)
                        ctx.lineTo(w * 0.76, h * 0.62)
                    } else {
                        ctx.moveTo(w * 0.24, h * 0.38)
                        ctx.lineTo(w * 0.50, h * 0.64)
                        ctx.lineTo(w * 0.76, h * 0.38)
                    }
                    ctx.stroke()
                }

                switch (root.iconName) {
                case "open":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.15, h * 0.35)
                    ctx.lineTo(w * 0.40, h * 0.35)
                    ctx.lineTo(w * 0.48, h * 0.45)
                    ctx.lineTo(w * 0.85, h * 0.45)
                    ctx.lineTo(w * 0.78, h * 0.76)
                    ctx.lineTo(w * 0.18, h * 0.76)
                    ctx.closePath()
                    ctx.stroke()
                    line(w * 0.18, h * 0.35, w * 0.18, h * 0.72)
                    break
                case "previous":
                    line(w * 0.18, h * 0.22, w * 0.18, h * 0.78)
                    triangle(w * 0.70, h * 0.22, w * 0.36, h * 0.50, w * 0.70, h * 0.78, true)
                    triangle(w * 0.88, h * 0.22, w * 0.54, h * 0.50, w * 0.88, h * 0.78, true)
                    break
                case "play":
                    triangle(w * 0.34, h * 0.20, w * 0.34, h * 0.80, w * 0.78, h * 0.50, true)
                    break
                case "pause":
                    ctx.fillRect(w * 0.30, h * 0.22, w * 0.13, h * 0.56)
                    ctx.fillRect(w * 0.57, h * 0.22, w * 0.13, h * 0.56)
                    break
                case "stop":
                    ctx.fillRect(w * 0.31, h * 0.31, w * 0.38, h * 0.38)
                    break
                case "next":
                    line(w * 0.82, h * 0.22, w * 0.82, h * 0.78)
                    triangle(w * 0.30, h * 0.22, w * 0.64, h * 0.50, w * 0.30, h * 0.78, true)
                    triangle(w * 0.12, h * 0.22, w * 0.46, h * 0.50, w * 0.12, h * 0.78, true)
                    break
                case "queue":
                    line(w * 0.18, h * 0.30, w * 0.62, h * 0.30)
                    line(w * 0.18, h * 0.50, w * 0.62, h * 0.50)
                    line(w * 0.18, h * 0.70, w * 0.48, h * 0.70)
                    line(w * 0.74, h * 0.48, w * 0.74, h * 0.84)
                    line(w * 0.56, h * 0.66, w * 0.92, h * 0.66)
                    break
                case "playlist":
                    line(w * 0.18, h * 0.30, w * 0.72, h * 0.30)
                    line(w * 0.18, h * 0.50, w * 0.72, h * 0.50)
                    line(w * 0.18, h * 0.70, w * 0.54, h * 0.70)
                    line(w * 0.78, h * 0.60, w * 0.78, h * 0.84)
                    line(w * 0.66, h * 0.72, w * 0.90, h * 0.72)
                    break
                case "metadata":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.18, h * 0.30)
                    ctx.lineTo(w * 0.48, h * 0.16)
                    ctx.lineTo(w * 0.82, h * 0.34)
                    ctx.lineTo(w * 0.76, h * 0.76)
                    ctx.lineTo(w * 0.30, h * 0.82)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.45, h * 0.34, w * 0.055, 0, Math.PI * 2)
                    ctx.fill()
                    line(w * 0.38, h * 0.56, w * 0.66, h * 0.52)
                    line(w * 0.36, h * 0.68, w * 0.62, h * 0.64)
                    break
                case "settings":
                    ctx.beginPath()
                    ctx.arc(w * 0.50, h * 0.50, w * 0.18, 0, Math.PI * 2)
                    ctx.stroke()
                    for (let i = 0; i < 6; ++i) {
                        const a = i * Math.PI / 3
                        line(w * 0.50 + Math.cos(a) * w * 0.31, h * 0.50 + Math.sin(a) * h * 0.31,
                             w * 0.50 + Math.cos(a) * w * 0.41, h * 0.50 + Math.sin(a) * h * 0.41)
                    }
                    break
                case "mute":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.16, h * 0.43)
                    ctx.lineTo(w * 0.34, h * 0.43)
                    ctx.lineTo(w * 0.52, h * 0.28)
                    ctx.lineTo(w * 0.52, h * 0.72)
                    ctx.lineTo(w * 0.34, h * 0.57)
                    ctx.lineTo(w * 0.16, h * 0.57)
                    ctx.closePath()
                    ctx.stroke()
                    line(w * 0.66, h * 0.36, w * 0.88, h * 0.64)
                    line(w * 0.88, h * 0.36, w * 0.66, h * 0.64)
                    break
                case "volume":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.16, h * 0.43)
                    ctx.lineTo(w * 0.34, h * 0.43)
                    ctx.lineTo(w * 0.52, h * 0.28)
                    ctx.lineTo(w * 0.52, h * 0.72)
                    ctx.lineTo(w * 0.34, h * 0.57)
                    ctx.lineTo(w * 0.16, h * 0.57)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.52, h * 0.50, w * 0.22, -0.62, 0.62)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.52, h * 0.50, w * 0.36, -0.58, 0.58)
                    ctx.stroke()
                    break
                case "sequential":
                    line(w * 0.18, h * 0.50, w * 0.72, h * 0.50)
                    triangle(w * 0.72, h * 0.32, w * 0.72, h * 0.68, w * 0.90, h * 0.50, true)
                    break
                case "repeatOne":
                case "repeatAll":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.28, h * 0.35)
                    ctx.lineTo(w * 0.72, h * 0.35)
                    ctx.lineTo(w * 0.72, h * 0.24)
                    ctx.lineTo(w * 0.90, h * 0.42)
                    ctx.lineTo(w * 0.72, h * 0.60)
                    ctx.lineTo(w * 0.72, h * 0.49)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(w * 0.72, h * 0.65)
                    ctx.lineTo(w * 0.28, h * 0.65)
                    ctx.lineTo(w * 0.28, h * 0.76)
                    ctx.lineTo(w * 0.10, h * 0.58)
                    ctx.lineTo(w * 0.28, h * 0.40)
                    ctx.lineTo(w * 0.28, h * 0.51)
                    ctx.stroke()
                    if (root.iconName === "repeatOne") {
                        ctx.font = "700 " + Math.round(w * 0.34) + "px sans-serif"
                        ctx.textAlign = "center"
                        ctx.textBaseline = "middle"
                        ctx.fillText("1", w * 0.50, h * 0.50)
                    }
                    break
                case "shuffle":
                    ctx.beginPath()
                    ctx.moveTo(w * 0.12, h * 0.32)
                    ctx.bezierCurveTo(w * 0.36, h * 0.32, w * 0.38, h * 0.68, w * 0.72, h * 0.68)
                    ctx.stroke()
                    triangle(w * 0.72, h * 0.50, w * 0.72, h * 0.86, w * 0.90, h * 0.68, true)
                    ctx.beginPath()
                    ctx.moveTo(w * 0.12, h * 0.68)
                    ctx.bezierCurveTo(w * 0.36, h * 0.68, w * 0.38, h * 0.32, w * 0.72, h * 0.32)
                    ctx.stroke()
                    triangle(w * 0.72, h * 0.14, w * 0.72, h * 0.50, w * 0.90, h * 0.32, true)
                    break
                case "moveUp":
                    chevron(true)
                    break
                case "moveDown":
                    chevron(false)
                    break
                case "remove":
                    line(w * 0.25, h * 0.25, w * 0.75, h * 0.75)
                    line(w * 0.75, h * 0.25, w * 0.25, h * 0.75)
                    break
                default:
                    line(w * 0.24, h * 0.50, w * 0.76, h * 0.50)
                    line(w * 0.50, h * 0.24, w * 0.50, h * 0.76)
                    break
                }
            }
        }
    }
}
