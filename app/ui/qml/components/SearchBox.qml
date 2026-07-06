import QtQuick
import QtQuick.Controls
import Opusora

TextField {
    id: root

    signal searchRequested(string query)

    implicitWidth: 292
    implicitHeight: 38
    placeholderText: (I18n.revision, I18n.t("search.placeholder"))
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"
    font.pixelSize: 13
    leftPadding: 16
    rightPadding: 16
    verticalAlignment: TextInput.AlignVCenter
    onAccepted: root.searchRequested(text)

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
            opacity: root.activeFocus ? 0.42 : 0.28
        }
    }
}
