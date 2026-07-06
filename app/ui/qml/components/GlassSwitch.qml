import QtQuick
import QtQuick.Controls
import Opusora

Switch {
    id: root

    implicitWidth: 48
    implicitHeight: 30
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 28
        x: 0
        y: Math.round((root.height - height) / 2)
        radius: height / 2
        color: root.checked
            ? Theme.accent
            : (root.hovered ? Theme.controlHover : Theme.controlFill)
        border.color: root.checked ? Qt.rgba(1, 1, 1, 0.34) : Theme.glassBorder
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Rectangle {
            width: 22
            height: 22
            x: root.checked ? parent.width - width - 3 : 3
            y: 3
            radius: width / 2
            color: "#ffffff"
            border.color: Qt.rgba(0, 0, 0, 0.10)
            border.width: 1

            Behavior on x {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    contentItem: Item {
        implicitWidth: root.indicator.implicitWidth
        implicitHeight: root.indicator.implicitHeight
    }
}
