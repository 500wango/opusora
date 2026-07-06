import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    function displayArtist(value) {
        return value && value.length > 0 ? value : (I18n.revision, I18n.t("metadata.unknownArtist"))
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: clearButton.left
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 16
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.queue"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    GlassButton {
        id: clearButton
        anchors.right: parent.right
        anchors.rightMargin: 28
        anchors.verticalCenter: heading.verticalCenter
        width: 118
        height: 34
        text: (I18n.revision, I18n.t("action.clearQueue"))
        enabled: PlaybackController.queueCount > 0
        onClicked: PlaybackController.clearQueue()
    }

    Text {
        id: queueMeta
        anchors.left: heading.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.rightMargin: 28
        anchors.topMargin: 8
        text: PlaybackController.queueCount + " " + (I18n.revision, I18n.t("detail.tracks"))
        color: Theme.textSecondary
        font.pixelSize: 13
        elide: Text.ElideRight
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: queueMeta.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.queue.title"))
        actionText: ""
        visible: PlaybackController.queueCount === 0
    }

    ListView {
        id: queueList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: queueMeta.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        model: PlaybackController.queueItems
        visible: PlaybackController.queueCount > 0
        spacing: 1

        delegate: Rectangle {
            id: queueDelegate

            property int queueItemIndex: modelData.index
            property var queueToken: modelData.queueToken
            property bool currentItem: modelData.current

            width: queueList.width
            height: 54
            radius: Theme.radiusSmall
            color: currentItem ? Theme.accentSoft : (mouseArea.containsMouse ? Theme.surface : "transparent")

            Rectangle {
                id: marker
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34
                radius: Theme.radiusSmall
                color: currentItem ? Theme.accent : (Theme.mode === "dark" ? "#2f3138" : "#e6e8ef")
                border.color: currentItem ? Theme.accent : Theme.hairline
                border.width: currentItem ? 0 : 1

                Text {
                    anchors.centerIn: parent
                    text: currentItem ? ">" : (queueDelegate.queueItemIndex + 1)
                    color: currentItem ? "#ffffff" : Theme.textSecondary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Text {
                id: titleText
                anchors.left: marker.right
                anchors.leftMargin: 14
                anchors.right: artistText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.title
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: currentItem ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }

            Text {
                id: artistText
                anchors.right: pathText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(120, queueList.width * 0.20)
                text: root.displayArtist(modelData.artist)
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                id: pathText
                anchors.right: actions.left
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(160, queueList.width * 0.26)
                text: modelData.filePath
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onDoubleClicked: PlaybackController.playQueueToken(queueDelegate.queueToken)
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                IconButton {
                    iconName: "play"
                    tooltipText: (I18n.revision, I18n.t("action.play"))
                    enabled: !queueDelegate.currentItem
                    onClicked: PlaybackController.playQueueToken(queueDelegate.queueToken)
                }

                IconButton {
                    iconName: "moveUp"
                    tooltipText: (I18n.revision, I18n.t("action.moveUp"))
                    enabled: queueDelegate.queueItemIndex > 0
                    onClicked: PlaybackController.moveQueueItem(queueDelegate.queueItemIndex, -1)
                }

                AddToPlaylistButton {
                    visible: modelData.trackId > 0
                    trackId: modelData.trackId
                }

                IconButton {
                    iconName: "moveDown"
                    tooltipText: (I18n.revision, I18n.t("action.moveDown"))
                    enabled: queueDelegate.queueItemIndex < PlaybackController.queueCount - 1
                    onClicked: PlaybackController.moveQueueItem(queueDelegate.queueItemIndex, 1)
                }

                IconButton {
                    iconName: "remove"
                    tooltipText: (I18n.revision, I18n.t("action.remove"))
                    onClicked: PlaybackController.removeQueueItem(queueDelegate.queueItemIndex)
                }
            }
        }
    }
}
