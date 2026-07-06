import QtQuick
import QtQuick.Controls
import Opusora

TextArea {
    id: root

    leftPadding: 14
    rightPadding: 14
    topPadding: 12
    bottomPadding: 12
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"
    font.pixelSize: 13
    clip: true

    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.activeFocus ? Theme.elevated : Theme.surface
        border.color: root.activeFocus ? Theme.accent : Theme.glassBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: Math.min(parent.height * 0.40, 56)
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: 0.22
        }
    }
}
