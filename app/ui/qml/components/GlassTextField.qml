import QtQuick
import QtQuick.Controls
import Opusora

TextField {
    id: root

    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"
    font.pixelSize: 13
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        radius: height / 2
        color: root.activeFocus ? Theme.controlHover : Theme.controlFill
        border.color: root.activeFocus ? Theme.accent : Theme.glassBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height / 2
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: 0.24
        }
    }
}
