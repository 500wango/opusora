import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property bool detailMode: false
    property string selectedGenreName: ""
    property int selectedGenreTrackCount: 0
    property int selectedGenreDurationMs: 0

    function formatDuration(ms) {
        if (!ms || ms <= 0) {
            return "--:--"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60
        const paddedSeconds = seconds < 10 ? "0" + seconds : seconds
        if (hours > 0) {
            const paddedMinutes = minutes < 10 ? "0" + minutes : minutes
            return hours + ":" + paddedMinutes + ":" + paddedSeconds
        }
        return minutes + ":" + paddedSeconds
    }

    function openGenre(name, trackCount, durationMs) {
        selectedGenreName = name
        selectedGenreTrackCount = trackCount
        selectedGenreDurationMs = durationMs
        GenreTrackModel.loadGenre(name)
        detailMode = true
    }

    function playFirstTrack() {
        PlaybackController.playQueue(GenreTrackModel.queueItems(), 0)
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.genres"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        visible: !root.detailMode
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.genres.title"))
        actionText: (I18n.revision, I18n.t("action.addFolder"))
        visible: !root.detailMode && GenreModel.count === 0
        onActionRequested: LibraryController.requestAddFolder()
    }

    ListView {
        id: genreList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        visible: !root.detailMode && GenreModel.count > 0
        model: GenreModel
        spacing: 1

        delegate: Rectangle {
            width: genreList.width
            height: 58
            radius: Theme.radiusSmall
            color: mouseArea.containsMouse ? Theme.accentSoft : "transparent"

            Rectangle {
                id: marker
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
                radius: Theme.radiusSmall
                color: Theme.mode === "dark" ? "#2f3138" : "#e6e8ef"
                border.color: Theme.hairline
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "#"
                    color: Theme.textSecondary
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }

            Text {
                anchors.left: marker.right
                anchors.leftMargin: 14
                anchors.right: countText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: model.name
                color: Theme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                id: countText
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 210
                text: model.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
                    + " - " + root.formatDuration(model.durationMs)
                color: Theme.textSecondary
                font.pixelSize: 13
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openGenre(model.name, model.trackCount, model.durationMs)
            }
        }
    }

    Item {
        id: detailView
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 22
        anchors.bottomMargin: 24
        visible: root.detailMode

        GlassButton {
            id: backButton
            anchors.left: parent.left
            anchors.top: parent.top
            width: 98
            height: 34
            text: "< " + (I18n.revision, I18n.t("action.back"))
            onClicked: root.detailMode = false
        }

        Item {
            id: genreHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: backButton.bottom
            anchors.topMargin: 20
            height: 116

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                text: root.selectedGenreName
                color: Theme.textPrimary
                font.pixelSize: 30
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                id: metaText
                anchors.left: titleText.left
                anchors.right: titleText.right
                anchors.top: titleText.bottom
                anchors.topMargin: 10
                text: root.selectedGenreTrackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
                    + " - " + root.formatDuration(root.selectedGenreDurationMs)
                color: Theme.textSecondary
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            GlassButton {
                id: playButton
                anchors.left: titleText.left
                anchors.top: metaText.bottom
                anchors.topMargin: 18
                width: 94
                height: 36
                text: (I18n.revision, I18n.t("action.play"))
                enabled: GenreTrackModel.count > 0
                onClicked: root.playFirstTrack()
            }

            GlassButton {
                id: addQueueButton
                anchors.left: playButton.right
                anchors.leftMargin: 10
                anchors.top: playButton.top
                width: 128
                height: 36
                text: (I18n.revision, I18n.t("action.addToQueue"))
                enabled: GenreTrackModel.count > 0
                onClicked: PlaybackController.appendQueueItems(GenreTrackModel.queueItems())
            }
        }

        ListView {
            id: trackList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: genreHeader.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: 18
            clip: true
            model: GenreTrackModel
            spacing: 1

            delegate: Rectangle {
                id: trackDelegate

                property int rowIndex: index

                width: trackList.width
                height: 48
                radius: Theme.radiusSmall
                color: trackMouseArea.containsMouse ? Theme.accentSoft : "transparent"

                Text {
                    id: trackTitle
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: albumText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.title
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    id: albumText
                    anchors.right: artistText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(140, trackList.width * 0.22)
                    text: model.album && model.album.length > 0 ? model.album : (I18n.revision, I18n.t("metadata.unknownAlbum"))
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Text {
                    id: artistText
                    anchors.right: durationText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(120, trackList.width * 0.20)
                    text: model.artist && model.artist.length > 0 ? model.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Text {
                    id: durationText
                    anchors.right: actions.left
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    text: root.formatDuration(model.durationMs)
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                }

                MouseArea {
                    id: trackMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: PlaybackController.playQueueItemByIdentity(
                        GenreTrackModel.queueItems(),
                        model.trackId,
                        model.filePath)
                }

                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    AddToQueueButton {
                        filePath: model.filePath
                        trackId: model.trackId
                        title: model.title
                        artist: model.artist
                        album: model.album
                        coverUrl: model.coverUrl
                    }

                    AddToPlaylistButton {
                        trackId: model.trackId
                    }
                }
            }
        }
    }
}
