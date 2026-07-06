import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property bool editingDetails: false
    property string draftTitle: ""
    property string draftArtist: ""
    property string draftAlbum: ""
    property string draftLyrics: ""
    property var syncedLyrics: []

    Component.onCompleted: loadCurrentDetails()

    Connections {
        target: PlaybackController

        function onCurrentTrackChanged() {
            root.editingDetails = false
            root.loadCurrentDetails()
            if (!OnlineMetadataService.busy) {
                OnlineMetadataService.clearMessages()
            }
        }
    }

    Connections {
        target: OnlineMetadataService

        function onLyricsFetched(trackId, lyrics, title, artist, album) {
            if (trackId !== PlaybackController.currentTrackId) {
                OnlineMetadataService.clearMessages()
                return
            }

            root.draftLyrics = lyrics
            root.parseSyncedLyrics()
        }
    }

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

    function displayArtist(value) {
        return value && value.length > 0 ? value : (I18n.revision, I18n.t("metadata.unknownArtist"))
    }

    function loadCurrentDetails() {
        draftTitle = PlaybackController.title
        draftArtist = PlaybackController.artist
        draftAlbum = PlaybackController.album
        draftLyrics = PlaybackController.currentTrackId > 0
            ? TrackModel.lyricsForTrack(PlaybackController.currentTrackId)
            : ""
        parseSyncedLyrics()
    }

    function onlineMessage() {
        const key = OnlineMetadataService.errorMessage.length > 0
            ? OnlineMetadataService.errorMessage
            : OnlineMetadataService.status
        return key.length > 0 ? (I18n.revision, I18n.t(key)) : ""
    }

    function refreshLibraryViews() {
        TrackModel.reload()
        AlbumTrackModel.reload()
        ArtistTrackModel.reload()
        GenreTrackModel.reload()
        PlaylistTrackModel.reload()
        AlbumModel.reload()
        ArtistModel.reload()
        GenreModel.reload()
        SearchController.reload()
        RecentAddedTrackModel.loadRecentAdded(8)
        RecentPlayedTrackModel.loadRecentPlayed(8)
        DuplicateTrackModel.loadDuplicates(100)
    }

    function saveCurrentDetails() {
        if (PlaybackController.currentTrackId <= 0) {
            return
        }

        if (TrackModel.saveTrackDetails(
                PlaybackController.currentTrackId,
                draftTitle,
                draftArtist,
                draftAlbum,
                draftLyrics)) {
            PlaybackController.updateCurrentTrackDisplay(draftTitle, draftArtist, draftAlbum)
            root.refreshLibraryViews()
            editingDetails = false
            parseSyncedLyrics()
        }
    }

    function parseSyncedLyrics() {
        const lines = draftLyrics.split(/\r?\n/)
        const parsed = []
        const pattern = /\[(\d{1,2}):(\d{2})(?:[.:](\d{1,3}))?\]/g
        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i]
            const text = line.replace(pattern, "").trim()
            pattern.lastIndex = 0
            let match = pattern.exec(line)
            while (match) {
                const minutes = Number(match[1])
                const seconds = Number(match[2])
                const fraction = match[3] ? Number(match[3].padEnd(3, "0").slice(0, 3)) : 0
                parsed.push({
                    timeMs: minutes * 60000 + seconds * 1000 + fraction,
                    text: text
                })
                match = pattern.exec(line)
            }
        }
        parsed.sort(function(a, b) { return a.timeMs - b.timeMs })
        syncedLyrics = parsed.filter(function(line) { return line.text.length > 0 })
    }

    function currentLyricIndex(positionMs) {
        if (syncedLyrics.length === 0) {
            return -1
        }

        let current = 0
        for (let i = 0; i < syncedLyrics.length; ++i) {
            if (syncedLyrics[i].timeMs <= positionMs) {
                current = i
            } else {
                break
            }
        }
        return current
    }

    function lyricLine(offset) {
        const index = currentLyricIndex(PlaybackController.positionMs) + offset
        if (index < 0 || index >= syncedLyrics.length) {
            return ""
        }
        return syncedLyrics[index].text
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.nowPlaying"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("player.noTrack"))
        actionText: (I18n.revision, I18n.t("action.openFile"))
        visible: PlaybackController.currentFilePath.length === 0
        onActionRequested: PlaybackController.requestOpenFile()
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 22
        anchors.bottomMargin: 24
        visible: PlaybackController.currentFilePath.length > 0

        Item {
            id: nowPanel
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.min(500, Math.max(340, parent.width * 0.44))

            CoverImage {
                id: cover
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: Math.min(260, parent.width)
                height: width
                radius: Theme.radiusSmall
                source: PlaybackController.coverUrl

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 36
                    text: PlaybackController.title
                    color: Theme.textSecondary
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    visible: cover.source.toString().length === 0
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    height: 42
                    spacing: 4
                    visible: PlaybackController.isPlaying

                    Repeater {
                        model: 18

                        Rectangle {
                            width: 5
                            height: 10 + ((index * 11) % 28)
                            radius: 2
                            anchors.bottom: parent.bottom
                            color: "#ffffff"
                            opacity: 0.72

                            SequentialAnimation on height {
                                loops: Animation.Infinite
                                running: PlaybackController.isPlaying
                                NumberAnimation {
                                    to: 8 + ((index * 17) % 34)
                                    duration: 260 + index * 18
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: 10 + ((index * 11) % 28)
                                    duration: 220 + index * 14
                                    easing.type: Easing.InOutQuad
                                }
                            }
                        }
                    }
                }
            }

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: cover.bottom
                anchors.topMargin: 20
                text: PlaybackController.title.length > 0 ? PlaybackController.title : (I18n.revision, I18n.t("player.noTrack"))
                color: Theme.textPrimary
                font.pixelSize: 26
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                visible: !root.editingDetails
            }

            Text {
                id: artistText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: titleText.bottom
                anchors.topMargin: 8
                text: root.displayArtist(PlaybackController.artist)
                color: Theme.textSecondary
                font.pixelSize: 15
                elide: Text.ElideRight
                visible: !root.editingDetails
            }

            Column {
                id: editorFields
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: cover.bottom
                anchors.topMargin: 14
                spacing: 8
                visible: root.editingDetails

                GlassTextField {
                    width: parent.width
                    text: root.draftTitle
                    placeholderText: (I18n.revision, I18n.t("metadata.title"))
                    onTextChanged: root.draftTitle = text
                }

                GlassTextField {
                    width: parent.width
                    text: root.draftArtist
                    placeholderText: (I18n.revision, I18n.t("metadata.artist"))
                    onTextChanged: root.draftArtist = text
                }

                GlassTextField {
                    width: parent.width
                    text: root.draftAlbum
                    placeholderText: (I18n.revision, I18n.t("metadata.album"))
                    onTextChanged: root.draftAlbum = text
                }
            }

            Text {
                id: timeText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: root.editingDetails ? editorFields.bottom : artistText.bottom
                anchors.topMargin: 12
                text: root.formatDuration(PlaybackController.positionMs) + " / " + root.formatDuration(PlaybackController.durationMs)
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                id: pathText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: timeText.bottom
                anchors.topMargin: 8
                text: PlaybackController.currentFilePath
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            Flow {
                id: editActions
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: pathText.bottom
                anchors.topMargin: 12
                spacing: 8
                height: childrenRect.height
                visible: PlaybackController.currentTrackId > 0

                GlassButton {
                    width: 92
                    height: 34
                    text: root.editingDetails
                        ? (I18n.revision, I18n.t("action.save"))
                        : (I18n.revision, I18n.t("action.edit"))
                    enabled: !root.editingDetails || root.draftTitle.trim().length > 0
                    onClicked: {
                        if (root.editingDetails) {
                            root.saveCurrentDetails()
                        } else {
                            root.loadCurrentDetails()
                            root.editingDetails = true
                        }
                    }
                }

                GlassButton {
                    width: 104
                    height: 34
                    text: OnlineMetadataService.busy && OnlineMetadataService.status === "online.status.fetchingLyrics"
                        ? (I18n.revision, I18n.t("online.status.fetchingLyrics"))
                        : (I18n.revision, I18n.t("action.fetchLyrics"))
                    enabled: !OnlineMetadataService.busy
                        && PlaybackController.currentTrackId > 0
                        && PlaybackController.title.trim().length > 0
                    onClicked: {
                        root.loadCurrentDetails()
                        OnlineMetadataService.fetchLyrics(
                            PlaybackController.currentTrackId,
                            root.draftTitle,
                            root.draftArtist,
                            root.draftAlbum,
                            PlaybackController.durationMs)
                    }
                }

                GlassButton {
                    width: 92
                    height: 34
                    text: (I18n.revision, I18n.t("action.cancel"))
                    visible: root.editingDetails
                    onClicked: {
                        root.editingDetails = false
                        root.loadCurrentDetails()
                    }
                }
            }

            Text {
                id: onlineStatusText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: editActions.bottom
                anchors.topMargin: 8
                text: root.onlineMessage()
                visible: text.length > 0
                color: OnlineMetadataService.errorMessage.length > 0 ? "#d94b4b" : Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            GlassPanel {
                id: syncedLyricsBox
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: onlineStatusText.visible ? onlineStatusText.bottom : editActions.bottom
                anchors.topMargin: 10
                height: 96
                radius: Theme.radiusMedium
                visible: PlaybackController.currentTrackId > 0 && root.syncedLyrics.length > 0 && !root.editingDetails

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        width: parent.width
                        text: root.lyricLine(-1)
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: root.lyricLine(0)
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: root.lyricLine(1)
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
            }

            ScrollView {
                id: lyricsScroll
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: syncedLyricsBox.visible
                    ? syncedLyricsBox.bottom
                    : (onlineStatusText.visible ? onlineStatusText.bottom : editActions.bottom)
                anchors.topMargin: 10
                anchors.bottom: parent.bottom
                visible: PlaybackController.currentTrackId > 0
                clip: true

                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                GlassTextArea {
                    id: lyricsBox
                    width: lyricsScroll.availableWidth
                    height: Math.max(lyricsScroll.availableHeight, contentHeight + topPadding + bottomPadding)
                    readOnly: !root.editingDetails
                    wrapMode: TextEdit.Wrap
                    text: root.draftLyrics
                    placeholderText: (I18n.revision, I18n.t("metadata.lyrics"))
                    onTextChanged: {
                        if (root.editingDetails) {
                            root.draftLyrics = text
                            root.parseSyncedLyrics()
                        }
                    }
                }
            }
        }

        GlassPanel {
            id: queuePanel
            anchors.left: nowPanel.right
            anchors.leftMargin: 28
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: Theme.radiusLarge

            Row {
                id: queueHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.topMargin: 16
                height: 38
                spacing: 10

                Text {
                    width: queueHeader.width - clearButton.width - 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: (I18n.revision, I18n.t("page.queue")) + " (" + PlaybackController.queueCount + ")"
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                GlassButton {
                    id: clearButton
                    width: 118
                    height: 34
                    text: (I18n.revision, I18n.t("action.clearQueue"))
                    enabled: PlaybackController.queueCount > 0
                    onClicked: PlaybackController.clearQueue()
                }
            }

            ListView {
                id: queueList
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: queueHeader.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 12
                anchors.bottomMargin: 8
                clip: true
                model: PlaybackController.queueItems
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

                    Text {
                        id: indexText
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 34
                        text: currentItem ? ">" : (queueDelegate.queueItemIndex + 1)
                        color: currentItem ? Theme.accent : Theme.textSecondary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        id: trackTitle
                        anchors.left: indexText.right
                        anchors.leftMargin: 14
                        anchors.right: actions.left
                        anchors.rightMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        text: modelData.title
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: currentItem ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: trackTitle.left
                        anchors.right: trackTitle.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        text: root.displayArtist(modelData.artist)
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
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

                        AddToPlaylistButton {
                            visible: modelData.trackId > 0
                            trackId: modelData.trackId
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
    }
}
