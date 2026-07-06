import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property string title
    property string actionText
    signal actionRequested()

    implicitHeight: 220

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width, 360)
        spacing: 16

        Text {
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: 20
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        GlassButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.actionText.length > 0
            text: root.actionText
            onClicked: root.actionRequested()
        }
    }
}
