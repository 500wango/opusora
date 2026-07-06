import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property bool detailMode: false
    property int selectedPlaylistId: 0
    property string selectedPlaylistName: ""
    property int selectedPlaylistTrackCount: 0
    property int selectedPlaylistDurationMs: 0

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

    function createPlaylist() {
        const name = nameField.text.trim()
        if (name.length === 0) {
            return
        }
        if (PlaylistModel.createPlaylist(name)) {
            nameField.text = ""
        }
    }

    function openRenameDialog(playlistId, name) {
        renameDialog.playlistId = playlistId
        renameField.text = name
        renameDialog.open()
    }

    function renamePlaylist() {
        const name = renameField.text.trim()
        if (renameDialog.playlistId <= 0 || name.length === 0) {
            return
        }

        if (PlaylistModel.renamePlaylist(renameDialog.playlistId, name)) {
            if (root.selectedPlaylistId === renameDialog.playlistId) {
                root.selectedPlaylistName = name
            }
            renameDialog.close()
        }
    }

    function openPlaylist(playlistId, name, trackCount, durationMs) {
        selectedPlaylistId = playlistId
        selectedPlaylistName = name
        selectedPlaylistTrackCount = trackCount
        selectedPlaylistDurationMs = durationMs
        PlaylistTrackModel.loadPlaylist(playlistId)
        detailMode = true
    }

    function playFirstTrack() {
        PlaybackController.playQueue(PlaylistTrackModel.queueItems(), 0)
    }

    function removeEntryFromPlaylist(entryId) {
        if (PlaylistModel.removeEntry(selectedPlaylistId, entryId)) {
            PlaylistTrackModel.loadPlaylist(selectedPlaylistId)
        }
    }

    function movePlaylistEntry(entryId, direction) {
        if (PlaylistModel.moveEntry(selectedPlaylistId, entryId, direction)) {
            PlaylistTrackModel.loadPlaylist(selectedPlaylistId)
        }
    }

    Dialog {
        id: renameDialog

        property int playlistId: 0

        modal: true
        title: (I18n.revision, I18n.t("action.rename"))
        width: Math.min(380, root.width - 56)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
        background: GlassPanel {
            radius: Theme.radiusLarge
            fill: Theme.elevated
        }

        contentItem: Column {
            spacing: 14

            GlassTextField {
                id: renameField
                width: renameDialog.width - 48
                height: 36
                color: Theme.textPrimary
                placeholderText: (I18n.revision, I18n.t("playlist.namePlaceholder"))
                placeholderTextColor: Theme.textSecondary
                selectionColor: Theme.accent
                selectedTextColor: "#ffffff"
                font.pixelSize: 13
                verticalAlignment: TextInput.AlignVCenter
                onAccepted: root.renamePlaylist()

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.background
                    border.color: renameField.activeFocus ? Theme.accent : Theme.hairline
                    border.width: 1
                }
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                GlassButton {
                    width: 86
                    height: 34
                    text: (I18n.revision, I18n.t("action.cancel"))
                    onClicked: renameDialog.close()
                }

                GlassButton {
                    width: 86
                    height: 34
                    text: (I18n.revision, I18n.t("action.save"))
                    enabled: renameField.text.trim().length > 0
                    onClicked: root.renamePlaylist()
                }
            }
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
        text: (I18n.revision, I18n.t("page.playlists"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        visible: !root.detailMode
    }

    GlassPanel {
        id: createPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        height: 58
        radius: Theme.radiusLarge
        visible: !root.detailMode

        GlassTextField {
            id: nameField
            anchors.left: parent.left
            anchors.right: importButton.left
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            height: 34
            placeholderText: (I18n.revision, I18n.t("playlist.namePlaceholder"))
            color: Theme.textPrimary
            placeholderTextColor: Theme.textSecondary
            selectionColor: Theme.accent
            selectedTextColor: "#ffffff"
            font.pixelSize: 13
            verticalAlignment: TextInput.AlignVCenter
            onAccepted: root.createPlaylist()

            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.background
                border.color: nameField.activeFocus ? Theme.accent : Theme.hairline
                border.width: 1
            }
        }

        GlassButton {
            id: createButton
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 98
            height: 34
            text: (I18n.revision, I18n.t("action.create"))
            enabled: nameField.text.trim().length > 0
            onClicked: root.createPlaylist()
        }

        GlassButton {
            id: importButton
            anchors.right: createButton.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 98
            height: 34
            text: (I18n.revision, I18n.t("action.import"))
            onClicked: PlaylistModel.requestImportM3u()
        }
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: createPanel.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.playlists.title"))
        actionText: ""
        visible: !root.detailMode && PlaylistModel.count === 0
    }

    ListView {
        id: playlistList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: createPanel.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 18
        anchors.bottomMargin: 24
        clip: true
        visible: !root.detailMode && PlaylistModel.count > 0
        model: PlaylistModel
        spacing: 1

        delegate: Rectangle {
            id: playlistDelegate

            width: playlistList.width
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
                    text: ">"
                    color: Theme.textSecondary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }

            Text {
                id: nameText
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
                anchors.right: playlistActions.left
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 190
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
                onClicked: root.openPlaylist(model.playlistId, model.name, model.trackCount, model.durationMs)
            }

            Row {
                id: playlistActions
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                GlassButton {
                    width: 82
                    height: 32
                    text: (I18n.revision, I18n.t("action.rename"))
                    onClicked: root.openRenameDialog(model.playlistId, model.name)
                }

                GlassButton {
                    width: 82
                    height: 32
                    text: (I18n.revision, I18n.t("action.remove"))
                    onClicked: PlaylistModel.deletePlaylist(model.playlistId)
                }
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
            id: playlistHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: backButton.bottom
            anchors.topMargin: 20
            height: 126

            Text {
                id: detailTitle
                anchors.left: parent.left
                anchors.right: renameDetailButton.left
                anchors.rightMargin: 14
                anchors.top: parent.top
                text: root.selectedPlaylistName
                color: Theme.textPrimary
                font.pixelSize: 30
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            GlassButton {
                id: renameDetailButton
                anchors.right: parent.right
                anchors.top: parent.top
                width: 94
                height: 34
                text: (I18n.revision, I18n.t("action.rename"))
                onClicked: root.openRenameDialog(root.selectedPlaylistId, root.selectedPlaylistName)
            }

            Text {
                id: detailMeta
                anchors.left: detailTitle.left
                anchors.right: parent.right
                anchors.top: detailTitle.bottom
                anchors.topMargin: 10
                text: PlaylistTrackModel.count + " " + (I18n.revision, I18n.t("detail.tracks"))
                    + " - " + root.formatDuration(PlaylistTrackModel.totalDurationMs)
                color: Theme.textSecondary
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            GlassButton {
                id: playButton
                anchors.left: detailTitle.left
                anchors.top: detailMeta.bottom
                anchors.topMargin: 18
                width: 94
                height: 36
                text: (I18n.revision, I18n.t("action.play"))
                enabled: PlaylistTrackModel.count > 0
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
                enabled: PlaylistTrackModel.count > 0
                onClicked: PlaybackController.appendQueueItems(PlaylistTrackModel.queueItems())
            }

            GlassButton {
                id: exportButton
                anchors.left: addQueueButton.right
                anchors.leftMargin: 10
                anchors.top: playButton.top
                width: 94
                height: 36
                text: (I18n.revision, I18n.t("action.export"))
                enabled: PlaylistTrackModel.count > 0
                onClicked: PlaylistModel.requestExportM3u(root.selectedPlaylistId, root.selectedPlaylistName)
            }
        }

        EmptyState {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: playlistHeader.bottom
            anchors.bottom: parent.bottom
            title: (I18n.revision, I18n.t("empty.songs.title"))
            actionText: ""
            visible: PlaylistTrackModel.count === 0
        }

        ListView {
            id: trackList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: playlistHeader.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: 18
            clip: true
            model: PlaylistTrackModel
            visible: PlaylistTrackModel.count > 0
            spacing: 1

            delegate: Rectangle {
                id: trackDelegate

                property int trackIdValue: model.trackId
                property var playlistEntryIdValue: model.playlistEntryId
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
                    anchors.right: trackActions.left
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
                        PlaylistTrackModel.queueItems(),
                        model.trackId,
                        model.filePath)
                }

                Row {
                    id: trackActions
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

                    IconButton {
                        iconName: "moveUp"
                        tooltipText: (I18n.revision, I18n.t("action.moveUp"))
                        enabled: index > 0
                        onClicked: root.movePlaylistEntry(trackDelegate.playlistEntryIdValue, -1)
                    }

                    IconButton {
                        iconName: "moveDown"
                        tooltipText: (I18n.revision, I18n.t("action.moveDown"))
                        enabled: index < PlaylistTrackModel.count - 1
                        onClicked: root.movePlaylistEntry(trackDelegate.playlistEntryIdValue, 1)
                    }

                    IconButton {
                        iconName: "remove"
                        tooltipText: (I18n.revision, I18n.t("action.remove"))
                        onClicked: root.removeEntryFromPlaylist(trackDelegate.playlistEntryIdValue)
                    }
                }
            }
        }
    }
}
