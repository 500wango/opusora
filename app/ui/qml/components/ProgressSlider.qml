import QtQuick
import QtQuick.Controls
import Opusora

Slider {
    id: root

    property real trackHeight: 6
    property real handleSize: 16

    from: 0
    to: 1
    value: 0
    live: false

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: root.trackHeight
        radius: height / 2
        color: Theme.mode === "dark" ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0.16, 0.22, 0.32, 0.12)

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.accentStrong }
                GradientStop { position: 1.0; color: Theme.accent }
            }
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.pressed || root.hovered ? root.handleSize + 2 : root.handleSize
        height: width
        radius: width / 2
        color: Theme.mode === "dark" ? "#f6f7fa" : "#ffffff"
        border.color: Theme.glassBorder
        border.width: 1

        Behavior on width {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }
}
