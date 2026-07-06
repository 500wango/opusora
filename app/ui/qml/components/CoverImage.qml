import QtQuick
import Opusora

Rectangle {
    id: root

    property url source

    color: Theme.mode === "dark" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.95, 0.97, 1.0, 0.70)
    border.color: Theme.glassBorder
    border.width: 1
    clip: true

    Image {
        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectCrop
        visible: root.source.toString().length > 0 && status === Image.Ready
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.44
        height: width
        radius: width / 2
        color: Theme.controlFill
        border.color: Theme.hairline
        border.width: 1
        visible: root.source.toString().length === 0

        Text {
            anchors.centerIn: parent
            text: "♪"
            color: Theme.textSecondary
            font.pixelSize: Math.max(12, parent.width * 0.42)
        }
    }
}
