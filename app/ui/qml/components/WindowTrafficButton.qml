import QtQuick

Rectangle {
    id: root

    signal clicked()

    property color buttonColor: "#ff5f57"

    width: 12
    height: 12
    radius: 6
    color: mouseArea.pressed ? Qt.darker(buttonColor, 1.2) : buttonColor
    border.color: "#00000022"
    border.width: 1

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        height: parent.height / 2
        radius: parent.radius
        color: "#ffffff99"
        opacity: mouseArea.containsMouse ? 0.58 : 0.30
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
