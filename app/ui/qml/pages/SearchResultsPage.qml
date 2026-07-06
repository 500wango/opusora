import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    function formatDuration(ms) {
        if (!ms || ms <= 0) {
            return "--:--"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds)
    }

    function resultSubtitle(item) {
        if (item.type === "track") {
            const artist = item.artist && item.artist.length > 0 ? item.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
            const album = item.album && item.album.length > 0 ? item.album : (I18n.revision, I18n.t("metadata.unknownAlbum"))
            return artist + " - " + album + " - " + root.formatDuration(item.durationMs)
        }
        if (item.type === "album") {
            const albumArtist = item.artist && item.artist.length > 0 ? item.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
            return albumArtist + " - " + item.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
        }
        if (item.type === "artist") {
            return item.albumCount + " " + (I18n.revision, I18n.t("detail.albums"))
                + " - " + item.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
        }
        if (item.type === "playlist") {
            return item.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
                + " - " + root.formatDuration(item.durationMs)
        }
        return ""
    }

    function resultQueueItems(item) {
        return SearchController.queueItemsForResult(item.type, item.id)
    }

    function playResult(item) {
        const items = root.resultQueueItems(item)
        if (items.length > 0) {
            PlaybackController.playQueue(items, 0)
        }
    }

    function appendResult(item) {
        const items = root.resultQueueItems(item)
        if (items.length > 0) {
            PlaybackController.appendQueueItems(items)
        }
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.search"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    Text {
        id: summary
        anchors.left: heading.left
        anchors.right: heading.right
        anchors.top: heading.bottom
        anchors.topMargin: 8
        text: SearchController.query.length > 0
            ? SearchController.query + " - " + SearchController.totalCount + " " + (I18n.revision, I18n.t("search.results"))
            : ""
        color: Theme.textSecondary
        font.pixelSize: 13
        elide: Text.ElideRight
        visible: SearchController.query.length > 0
        height: visible ? implicitHeight : 0
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: summary.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.search.title"))
        actionText: ""
        visible: SearchController.totalCount === 0
    }

    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: summary.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        model: SearchController.groupedItems
        visible: SearchController.totalCount > 0
        spacing: 1

        delegate: Rectangle {
            id: resultDelegate

            width: listView.width
            height: modelData.type === "section" ? 34 : 56
            radius: Theme.radiusSmall
            color: modelData.type === "section" ? "transparent" : (mouseArea.containsMouse ? Theme.accentSoft : "transparent")

            Text {
                id: sectionText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.type === "section"
                    ? (I18n.revision, I18n.t(modelData.labelKey)) + " (" + modelData.count + ")"
                    : ""
                color: Theme.textPrimary
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                visible: modelData.type === "section"
            }

            CoverImage {
                id: cover
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 38
                height: 38
                radius: Theme.radiusSmall
                source: modelData.coverUrl ? modelData.coverUrl : ""
                visible: modelData.type !== "section"
            }

            Text {
                id: titleText
                anchors.left: cover.right
                anchors.leftMargin: 12
                anchors.right: actions.left
                anchors.rightMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 8
                text: modelData.title ? modelData.title : ""
                color: Theme.textPrimary
                font.pixelSize: 14
                elide: Text.ElideRight
                visible: modelData.type !== "section"
            }

            Text {
                id: subtitleText
                anchors.left: titleText.left
                anchors.right: actions.left
                anchors.rightMargin: 16
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                text: root.resultSubtitle(modelData)
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
                visible: modelData.type !== "section"
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: modelData.type !== "section"
                onDoubleClicked: root.playResult(modelData)
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                visible: modelData.type !== "section"

                IconButton {
                    iconName: "play"
                    tooltipText: (I18n.revision, I18n.t("action.play"))
                    onClicked: root.playResult(modelData)
                }

                IconButton {
                    iconName: "queue"
                    tooltipText: (I18n.revision, I18n.t("action.addToQueue"))
                    onClicked: root.appendResult(modelData)
                }

                AddToPlaylistButton {
                    visible: modelData.type === "track"
                    trackId: modelData.id
                }
            }
        }
    }
}
