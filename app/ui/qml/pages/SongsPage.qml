import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property var selectedTrackIds: ({})
    property int selectedCountValue: 0

    function formatDuration(ms) {
        if (!ms || ms <= 0) {
            return "--:--"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds)
    }

    function selectedLabel() {
        if (selectedCountValue <= 0) {
            return TrackModel.count + " " + (I18n.revision, I18n.t("detail.tracks"))
        }
        if (I18n.language === "zh-CN") {
            return selectedCountValue + (I18n.revision, I18n.t("selection.selected"))
        }
        return selectedCountValue + " " + (I18n.revision, I18n.t("selection.selected"))
    }

    function isSelected(trackId) {
        return !!selectedTrackIds[trackId]
    }

    function setTrackSelected(trackId, selected) {
        if (trackId <= 0) {
            return
        }

        const nextSelection = {}
        for (const key in selectedTrackIds) {
            if (selectedTrackIds[key]) {
                nextSelection[key] = true
            }
        }

        if (selected) {
            nextSelection[trackId] = true
        } else {
            delete nextSelection[trackId]
        }

        selectedTrackIds = nextSelection
        selectedCountValue = Object.keys(selectedTrackIds).length
    }

    function toggleTrackSelected(trackId) {
        setTrackSelected(trackId, !isSelected(trackId))
    }

    function selectedIds() {
        const ids = []
        const items = TrackModel.queueItems()
        for (let index = 0; index < items.length; ++index) {
            const trackId = Number(items[index].trackId)
            if (trackId > 0 && selectedTrackIds[trackId]) {
                ids.push(trackId)
            }
        }
        return ids
    }

    function clearSelection() {
        selectedTrackIds = ({})
        selectedCountValue = 0
    }

    function selectAll() {
        const nextSelection = {}
        const items = TrackModel.queueItems()
        for (let index = 0; index < items.length; ++index) {
            const trackId = Number(items[index].trackId)
            if (trackId > 0) {
                nextSelection[trackId] = true
            }
        }
        selectedTrackIds = nextSelection
        selectedCountValue = Object.keys(selectedTrackIds).length
    }

    function addSelectedTracksToPlaylist(playlistId) {
        const added = PlaylistModel.addTracks(playlistId, selectedIds())
        if (added > 0) {
            clearSelection()
        }
    }

    Connections {
        target: TrackModel
        function onCountChanged() {
            root.clearSelection()
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
        text: (I18n.revision, I18n.t("page.songs"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.songs.title"))
        actionText: (I18n.revision, I18n.t("action.addFolder"))
        visible: TrackModel.count === 0
        onActionRequested: LibraryController.requestAddFolder()
    }

    Item {
        id: selectionBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 14
        height: 36
        visible: TrackModel.count > 0

        Text {
            anchors.left: parent.left
            anchors.right: bulkActions.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: root.selectedLabel()
            color: Theme.textSecondary
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Row {
            id: bulkActions
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            GlassButton {
                width: 92
                height: 32
                text: (I18n.revision, I18n.t("action.selectAll"))
                enabled: TrackModel.count > 0 && root.selectedCountValue < TrackModel.count
                onClicked: root.selectAll()
            }

            GlassButton {
                width: 104
                height: 32
                text: (I18n.revision, I18n.t("action.clearSelection"))
                enabled: root.selectedCountValue > 0
                onClicked: root.clearSelection()
            }

            GlassButton {
                id: bulkAddButton
                width: 136
                height: 32
                text: (I18n.revision, I18n.t("action.addToPlaylist"))
                enabled: root.selectedCountValue > 0 && PlaylistModel.count > 0
                onClicked: bulkPlaylistMenu.open()

                Menu {
                    id: bulkPlaylistMenu
                    y: bulkAddButton.height + 6

                    Repeater {
                        model: PlaylistModel

                        delegate: MenuItem {
                            text: model.name
                            onTriggered: root.addSelectedTracksToPlaylist(model.playlistId)
                        }
                    }
                }
            }
        }
    }

    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: selectionBar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 12
        anchors.bottomMargin: 24
        clip: true
        model: TrackModel
        visible: TrackModel.count > 0
        spacing: 1

        delegate: Rectangle {
            id: trackDelegate

            property int trackIdValue: model.trackId
            property int rowIndex: index
            property bool selected: root.isSelected(trackIdValue)

            width: listView.width
            height: 48
            radius: Theme.radiusSmall
            color: trackDelegate.selected
                ? Theme.accentSoft
                : (mouseArea.containsMouse ? Theme.controlHover : "transparent")

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.leftMargin: 44
                anchors.right: artistText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: model.title
                color: Theme.textPrimary
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            Text {
                id: artistText
                anchors.right: albumText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(120, listView.width * 0.22)
                text: model.artist && model.artist.length > 0 ? model.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                id: albumText
                anchors.right: durationText.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(120, listView.width * 0.22)
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
                width: 58
                text: root.formatDuration(model.durationMs)
                color: Theme.textSecondary
                font.pixelSize: 13
                horizontalAlignment: Text.AlignRight
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (root.selectedCountValue > 0) {
                        root.toggleTrackSelected(model.trackId)
                    }
                }
                onDoubleClicked: PlaybackController.playQueueItemByIdentity(
                    TrackModel.queueItems(),
                    model.trackId,
                    model.filePath)
            }

            CheckBox {
                id: selectionBox
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                checked: trackDelegate.selected
                hoverEnabled: true
                onToggled: root.setTrackSelected(trackDelegate.trackIdValue, checked)

                indicator: Rectangle {
                    x: Math.round((selectionBox.width - width) / 2)
                    y: Math.round((selectionBox.height - height) / 2)
                    width: 18
                    height: 18
                    radius: 5
                    color: selectionBox.checked
                        ? Theme.accent
                        : (selectionBox.hovered ? Theme.controlHover : Theme.controlFill)
                    border.color: selectionBox.checked ? Theme.accentStrong : Theme.hairline
                    border.width: 1

                    Canvas {
                        anchors.centerIn: parent
                        width: 10
                        height: 8
                        visible: selectionBox.checked
                        property bool watchedChecked: selectionBox.checked
                        onWatchedCheckedChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#ffffff"
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(width * 0.12, height * 0.48)
                            ctx.lineTo(width * 0.42, height * 0.78)
                            ctx.lineTo(width * 0.88, height * 0.18)
                            ctx.stroke()
                        }
                    }
                }

                contentItem: Item {}
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
                    trackId: trackDelegate.trackIdValue
                }

                IconButton {
                    iconName: "metadata"
                    tooltipText: (I18n.revision, I18n.t("action.fetchMetadata"))
                    enabled: model.trackId > 0
                        && model.title.trim().length > 0
                        && !OnlineMetadataService.busy
                    onClicked: OnlineMetadataService.fetchMetadata(
                        model.trackId,
                        model.title,
                        model.artist,
                        model.album,
                        model.durationMs)
                }
            }
        }
    }
}
