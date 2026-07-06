import QtQuick
import Opusora

Item {
    id: root

    default property alias contentData: content.data

    property int radius: Theme.radiusMedium
    property color fill: Theme.surface
    property color borderColor: Theme.glassBorder
    property real borderWidth: 1
    property bool shadowVisible: true
    property bool highlightVisible: true
    property bool clipContent: false

    Rectangle {
        anchors.fill: panel
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        anchors.topMargin: 10
        radius: panel.radius
        color: Theme.shadow
        opacity: root.shadowVisible ? 0.42 : 0
        visible: opacity > 0
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: root.radius
        color: root.fill
        border.color: root.borderColor
        border.width: root.borderWidth
    }

    Rectangle {
        anchors.left: panel.left
        anchors.right: panel.right
        anchors.top: panel.top
        anchors.margins: 1
        height: Math.min(panel.height * 0.44, 62)
        radius: panel.radius
        color: Theme.glassHighlight
        opacity: root.highlightVisible ? 0.24 : 0
        visible: opacity > 0
    }

    Rectangle {
        anchors.fill: panel
        anchors.margins: 1
        radius: Math.max(0, panel.radius - 1)
        color: "transparent"
        border.color: Theme.innerStroke
        border.width: root.borderWidth > 0 ? 1 : 0
        opacity: 0.46
    }

    Item {
        id: content
        anchors.fill: parent
        clip: root.clipContent
    }
}
