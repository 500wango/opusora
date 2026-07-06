import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property bool detailMode: false
    property int selectedArtistId: 0
    property string selectedArtistName: ""
    property int selectedArtistAlbumCount: 0
    property int selectedArtistTrackCount: 0

    function formatDuration(ms) {
        if (!ms || ms <= 0) {
            return "--:--"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds)
    }

    function artistInitial(name) {
        if (!name || name.length === 0) {
            return "?"
        }
        return name.charAt(0).toUpperCase()
    }

    function openArtist(artistId, name, albumCount, trackCount) {
        selectedArtistId = artistId
        selectedArtistName = name && name.length > 0 ? name : (I18n.revision, I18n.t("metadata.unknownArtist"))
        selectedArtistAlbumCount = albumCount
        selectedArtistTrackCount = trackCount
        ArtistTrackModel.loadArtist(artistId)
        detailMode = true
    }

    function playFirstTrack() {
        PlaybackController.playQueue(ArtistTrackModel.queueItems(), 0)
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.artists"))
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
        title: (I18n.revision, I18n.t("empty.artists.title"))
        actionText: (I18n.revision, I18n.t("action.addFolder"))
        visible: !root.detailMode && ArtistModel.count === 0
        onActionRequested: LibraryController.requestAddFolder()
    }

    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        visible: !root.detailMode && ArtistModel.count > 0
        model: ArtistModel
        spacing: 1

        delegate: Rectangle {
            width: listView.width
            height: 58
            radius: Theme.radiusSmall
            color: mouseArea.containsMouse ? Theme.accentSoft : "transparent"

            Rectangle {
                id: avatar
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 38
                height: 38
                radius: width / 2
                color: Theme.mode === "dark" ? "#2f3138" : "#e6e8ef"
                border.color: Theme.hairline
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.artistInitial(model.name)
                    color: Theme.textSecondary
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
            }

            Text {
                anchors.left: avatar.right
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
                width: 230
                text: model.albumCount + " " + (I18n.revision, I18n.t("detail.albums"))
                    + " / " + model.trackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
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
                onClicked: root.openArtist(model.artistId, model.name, model.albumCount, model.trackCount)
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
            id: artistHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: backButton.bottom
            anchors.topMargin: 20
            height: 126

            Rectangle {
                id: detailAvatar
                anchors.left: parent.left
                anchors.top: parent.top
                width: 108
                height: 108
                radius: width / 2
                color: Theme.mode === "dark" ? "#2f3138" : "#e6e8ef"
                border.color: Theme.hairline
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.artistInitial(root.selectedArtistName)
                    color: Theme.textSecondary
                    font.pixelSize: 34
                    font.weight: Font.DemiBold
                }
            }

            Text {
                id: titleText
                anchors.left: detailAvatar.right
                anchors.leftMargin: 22
                anchors.right: parent.right
                anchors.top: detailAvatar.top
                text: root.selectedArtistName
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
                text: root.selectedArtistAlbumCount + " " + (I18n.revision, I18n.t("detail.albums"))
                    + " / " + root.selectedArtistTrackCount + " " + (I18n.revision, I18n.t("detail.tracks"))
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
                enabled: ArtistTrackModel.count > 0
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
                enabled: ArtistTrackModel.count > 0
                onClicked: PlaybackController.appendQueueItems(ArtistTrackModel.queueItems())
            }
        }

        ListView {
            id: trackList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: artistHeader.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: 18
            clip: true
            model: ArtistTrackModel
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
                    anchors.right: durationText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(160, trackList.width * 0.28)
                    text: model.album && model.album.length > 0 ? model.album : (I18n.revision, I18n.t("metadata.unknownAlbum"))
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
                        ArtistTrackModel.queueItems(),
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
