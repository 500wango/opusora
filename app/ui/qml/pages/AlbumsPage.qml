import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property bool detailMode: false
    property int selectedAlbumId: 0
    property string selectedAlbumTitle: ""
    property string selectedAlbumArtist: ""
    property url selectedAlbumCoverUrl
    property int selectedAlbumTrackCount: 0
    property int selectedAlbumDurationMs: 0

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

    function formatTrackIndex(discNumber, trackNumber, row) {
        if (trackNumber > 0 && discNumber > 1) {
            return discNumber + "-" + trackNumber
        }
        if (trackNumber > 0) {
            return trackNumber
        }
        return row + 1
    }

    function openAlbum(albumId, title, artist, coverUrl, trackCount, durationMs) {
        selectedAlbumId = albumId
        selectedAlbumTitle = title
        selectedAlbumArtist = artist && artist.length > 0 ? artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
        selectedAlbumCoverUrl = coverUrl
        selectedAlbumTrackCount = trackCount
        selectedAlbumDurationMs = durationMs
        AlbumTrackModel.loadAlbum(albumId)
        detailMode = true
    }

    function playFirstTrack() {
        PlaybackController.playQueue(AlbumTrackModel.queueItems(), 0)
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.albums"))
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
        title: (I18n.revision, I18n.t("empty.albums.title"))
        actionText: (I18n.revision, I18n.t("action.addFolder"))
        visible: !root.detailMode && AlbumModel.count === 0
        onActionRequested: LibraryController.requestAddFolder()
    }

    GridView {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        visible: !root.detailMode && AlbumModel.count > 0
        model: AlbumModel
        cellWidth: 186
        cellHeight: 238

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            CoverImage {
                id: cover
                anchors.left: parent.left
                anchors.top: parent.top
                width: 154
                height: 154
                radius: Theme.radiusSmall
                source: model.coverUrl

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    text: model.title
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    visible: cover.source.toString().length === 0
                }
            }

            Text {
                anchors.left: cover.left
                anchors.right: cover.right
                anchors.top: cover.bottom
                anchors.topMargin: 10
                text: model.title
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                anchors.left: cover.left
                anchors.right: cover.right
                anchors.top: cover.bottom
                anchors.topMargin: 30
                text: (model.artist && model.artist.length > 0 ? model.artist : (I18n.revision, I18n.t("metadata.unknownArtist")))
                    + " - " + model.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openAlbum(model.albumId, model.title, model.artist, model.coverUrl, model.trackCount, model.durationMs)
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

            background: Rectangle {
                radius: Theme.radiusSmall
                color: backButton.hovered ? Theme.accentSoft : "transparent"
                border.color: Theme.hairline
                border.width: 1
            }

            contentItem: Text {
                text: backButton.text
                color: Theme.textPrimary
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Item {
            id: albumHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: backButton.bottom
            anchors.topMargin: 20
            height: 144

            CoverImage {
                id: detailCover
                anchors.left: parent.left
                anchors.top: parent.top
                width: 132
                height: 132
                radius: Theme.radiusSmall
                source: root.selectedAlbumCoverUrl

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    text: root.selectedAlbumTitle
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    visible: detailCover.source.toString().length === 0
                }
            }

            Text {
                id: titleText
                anchors.left: detailCover.right
                anchors.leftMargin: 22
                anchors.right: parent.right
                anchors.top: detailCover.top
                text: root.selectedAlbumTitle
                color: Theme.textPrimary
                font.pixelSize: 30
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                id: artistText
                anchors.left: titleText.left
                anchors.right: titleText.right
                anchors.top: titleText.bottom
                anchors.topMargin: 8
                text: root.selectedAlbumArtist
                color: Theme.textSecondary
                font.pixelSize: 15
                elide: Text.ElideRight
            }

            Text {
                id: metaText
                anchors.left: titleText.left
                anchors.right: titleText.right
                anchors.top: artistText.bottom
                anchors.topMargin: 8
                text: root.selectedAlbumTrackCount + " " + (I18n.revision, I18n.t("detail.tracks")) + " - " + root.formatDuration(root.selectedAlbumDurationMs)
                color: Theme.textSecondary
                font.pixelSize: 13
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
                enabled: AlbumTrackModel.count > 0
                onClicked: root.playFirstTrack()

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: playButton.enabled ? Theme.accent : Theme.hairline
                }

                contentItem: Text {
                    text: playButton.text
                    color: playButton.enabled ? "#ffffff" : Theme.textSecondary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            GlassButton {
                id: addQueueButton
                anchors.left: playButton.right
                anchors.leftMargin: 10
                anchors.top: playButton.top
                width: 128
                height: 36
                text: (I18n.revision, I18n.t("action.addToQueue"))
                enabled: AlbumTrackModel.count > 0
                onClicked: PlaybackController.appendQueueItems(AlbumTrackModel.queueItems())
            }
        }

        ListView {
            id: trackList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: albumHeader.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: 18
            clip: true
            model: AlbumTrackModel
            spacing: 1

            delegate: Rectangle {
                id: trackDelegate

                property int rowIndex: index

                width: trackList.width
                height: 48
                radius: Theme.radiusSmall
                color: trackMouseArea.containsMouse ? Theme.accentSoft : "transparent"

                Text {
                    id: numberText
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 46
                    text: root.formatTrackIndex(model.discNumber, model.trackNumber, index)
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }

                Text {
                    id: trackTitle
                    anchors.left: numberText.right
                    anchors.leftMargin: 16
                    anchors.right: artistName.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.title
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    id: artistName
                    anchors.right: durationText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(140, trackList.width * 0.24)
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
                        AlbumTrackModel.queueItems(),
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
