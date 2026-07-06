import QtQuick
import QtQuick.Controls
import Opusora

Control {
    id: root

    signal clicked()

    property string text
    property bool selected: false

    implicitHeight: 40
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: 12
        color: root.selected
            ? (Theme.mode === "dark" ? Qt.rgba(0.40, 0.58, 1.0, 0.26) : Qt.rgba(1.0, 1.0, 1.0, 0.68))
            : (mouseArea.containsMouse ? Theme.controlFill : "transparent")
        border.color: root.selected ? Theme.glassBorder : "transparent"
        border.width: root.selected ? 1 : 0

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: root.selected ? 20 : 0
            radius: 2
            color: Theme.accent

            Behavior on height {
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
        }
    }

    contentItem: Item {
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            anchors.rightMargin: 12
            text: root.text
            color: root.selected ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 14
            font.weight: root.selected ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }

    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
}
