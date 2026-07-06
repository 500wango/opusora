import QtQuick
import QtQuick.Controls
import Opusora

Button {
    id: root

    property bool emphasized: highlighted
    property bool destructive: false
    property string iconText: ""

    implicitHeight: 36
    implicitWidth: Math.max(86, label.implicitWidth + 30)
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    background: Rectangle {
        radius: height / 2
        color: !root.enabled
            ? (Theme.mode === "dark" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.34))
            : root.destructive
                ? (root.down || root.hovered ? Qt.rgba(1.0, 0.25, 0.25, 0.28) : Qt.rgba(1.0, 0.25, 0.25, 0.15))
                : root.emphasized
                    ? (root.down ? Theme.accentStrong : Theme.accent)
                    : (root.down || root.hovered ? Theme.controlHover : Theme.controlFill)
        border.color: root.emphasized
            ? Qt.rgba(1, 1, 1, 0.36)
            : (root.destructive ? Qt.rgba(1.0, 0.30, 0.30, 0.38) : Theme.glassBorder)
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height / 2
            radius: parent.radius
            color: root.emphasized ? Qt.rgba(1, 1, 1, 0.32) : Theme.glassHighlight
            opacity: root.down ? 0.14 : 0.34
        }
    }

    contentItem: Text {
        id: label
        text: root.iconText.length > 0 && root.text.length > 0
            ? root.iconText + " " + root.text
            : (root.iconText.length > 0 ? root.iconText : root.text)
        color: !root.enabled
            ? Theme.textSecondary
            : (root.emphasized ? "#ffffff" : Theme.textPrimary)
        opacity: root.enabled ? 1.0 : 0.62
        font.pixelSize: 13
        font.weight: root.emphasized ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
