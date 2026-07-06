import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    readonly property int pageMargin: 28

    function scanStatusText(status) {
        switch (status) {
        case "scanning":
            return I18n.t("library.scan.scanning")
        case "completed":
            return I18n.t("library.scan.completed")
        case "cancelled":
            return I18n.t("library.scan.cancelled")
        case "error":
            return I18n.t("library.scan.error")
        default:
            return I18n.t("library.scan.idle")
        }
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) {
            return "--:--"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        Column {
            width: root.width
            spacing: 18

            Text {
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: 48
                topPadding: 24
                text: (I18n.revision, I18n.t("page.library"))
                color: Theme.textPrimary
                font.pixelSize: 30
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            EmptyState {
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: visible ? implicitHeight : 0
                title: (I18n.revision, I18n.t("empty.library.title"))
                actionText: (I18n.revision, I18n.t("action.addFolder"))
                visible: LibraryController.roots.length === 0
                onActionRequested: LibraryController.requestAddFolder()
            }

            GlassPanel {
                id: scanPanel
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: 104
                radius: Theme.radiusLarge

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.right: rebuildButton.left
                    anchors.rightMargin: 18
                    anchors.top: parent.top
                    anchors.topMargin: 18
                    text: (I18n.revision, I18n.t("library.scanStatus")) + ": " + root.scanStatusText(LibraryController.scanStatus)
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    spacing: 22

                    Text {
                        text: (I18n.revision, I18n.t("library.totalFiles")) + ": " + LibraryController.totalFiles
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }

                    Text {
                        text: (I18n.revision, I18n.t("library.audioFiles")) + ": " + LibraryController.audioFiles
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }

                    Text {
                        text: (I18n.revision, I18n.t("library.failedFiles")) + ": " + LibraryController.failedFiles
                        color: LibraryController.failedFiles > 0 ? "#dc2626" : Theme.textSecondary
                        font.pixelSize: 13
                    }
                }

                GlassButton {
                    id: rebuildButton
                    anchors.right: scanActionButton.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 104
                    height: 34
                    text: (I18n.revision, I18n.t("action.rebuild"))
                    enabled: LibraryController.roots.length > 0 && !LibraryController.isScanning
                    onClicked: LibraryController.rebuildLibrary()
                }

                GlassButton {
                    id: scanActionButton
                    anchors.right: addButton.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 104
                    height: 34
                    text: LibraryController.isScanning
                        ? (I18n.revision, I18n.t("action.cancel"))
                        : (I18n.revision, I18n.t("action.rescan"))
                    enabled: LibraryController.roots.length > 0
                    onClicked: {
                        if (LibraryController.isScanning) {
                            LibraryController.cancelScan()
                        } else {
                            LibraryController.scanAll()
                        }
                    }
                }

                GlassButton {
                    id: addButton
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 122
                    height: 34
                    text: (I18n.revision, I18n.t("action.addFolder"))
                    enabled: !LibraryController.isScanning
                    onClicked: LibraryController.requestAddFolder()
                }
            }

            GlassPanel {
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: 74
                radius: Theme.radiusLarge

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 160
                    text: (I18n.revision, I18n.t("library.quickActions"))
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 196
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    GlassButton {
                        width: 122
                        height: 34
                        text: (I18n.revision, I18n.t("action.addFolder"))
                        enabled: !LibraryController.isScanning
                        onClicked: LibraryController.requestAddFolder()
                    }

                    GlassButton {
                        width: 112
                        height: 34
                        text: (I18n.revision, I18n.t("action.openFile"))
                        onClicked: PlaybackController.requestOpenFile()
                    }

                    GlassButton {
                        width: 104
                        height: 34
                        text: (I18n.revision, I18n.t("action.rescan"))
                        enabled: LibraryController.roots.length > 0 && !LibraryController.isScanning
                        onClicked: LibraryController.scanAll()
                    }
                }
            }

            GlassPanel {
                id: rootsPanel
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: visible ? Math.min(254, 54 + LibraryController.roots.length * 50) : 0
                radius: Theme.radiusLarge
                clipContent: true
                visible: LibraryController.roots.length > 0

                Text {
                    id: rootsHeading
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    text: (I18n.revision, I18n.t("settings.libraryFolders"))
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                ListView {
                    id: rootsList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: rootsHeading.bottom
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    clip: true
                    model: LibraryController.roots

                    delegate: Rectangle {
                        width: rootsList.width
                        height: 48
                        radius: Theme.radiusSmall
                        color: rootMouseArea.containsMouse ? Theme.accentSoft : "transparent"

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.right: scanRootButton.left
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                        }

                        GlassButton {
                            id: scanRootButton
                            anchors.right: removeButton.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 82
                            height: 32
                            text: (I18n.revision, I18n.t("action.scanFolder"))
                            enabled: !LibraryController.isScanning
                            onClicked: LibraryController.scanRoot(modelData)
                        }

                        GlassButton {
                            id: removeButton
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 82
                            height: 32
                            text: (I18n.revision, I18n.t("action.remove"))
                            enabled: !LibraryController.isScanning
                            onClicked: LibraryController.removeRoot(modelData)
                        }

                        MouseArea {
                            id: rootMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            Row {
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: 296
                spacing: 18

                GlassPanel {
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    radius: Theme.radiusLarge

                    Text {
                        id: recentAddedHeading
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.top: parent.top
                        anchors.topMargin: 16
                        text: (I18n.revision, I18n.t("library.recentAdded"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.centerIn: recentAddedList
                        width: recentAddedList.width - 24
                        text: (I18n.revision, I18n.t("empty.recentAdded.title"))
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: RecentAddedTrackModel.count === 0
                    }

                    ListView {
                        id: recentAddedList
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: recentAddedHeading.bottom
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        anchors.topMargin: 10
                        clip: true
                        model: RecentAddedTrackModel
                        visible: RecentAddedTrackModel.count > 0

                        delegate: Rectangle {
                            id: recentAddedDelegate

                            property int rowIndex: index

                            width: recentAddedList.width
                            height: 52
                            radius: Theme.radiusSmall
                            color: recentAddedMouse.containsMouse ? Theme.accentSoft : "transparent"

                            Text {
                                id: recentAddedTitle
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: recentAddedDuration.left
                                anchors.rightMargin: 12
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                text: model.title
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                anchors.left: recentAddedTitle.left
                                anchors.right: recentAddedTitle.right
                                anchors.top: recentAddedTitle.bottom
                                anchors.topMargin: 3
                                text: model.artist && model.artist.length > 0 ? model.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Text {
                                id: recentAddedDuration
                                anchors.right: recentAddedQueue.left
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: 54
                                text: root.formatDuration(model.durationMs)
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                            }

                            AddToQueueButton {
                                id: recentAddedQueue
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                filePath: model.filePath
                                trackId: model.trackId
                                title: model.title
                                artist: model.artist
                                album: model.album
                                coverUrl: model.coverUrl
                            }

                            MouseArea {
                                id: recentAddedMouse
                                anchors.fill: parent
                                anchors.rightMargin: 42
                                hoverEnabled: true
                                onDoubleClicked: PlaybackController.playQueueItemByIdentity(
                                    RecentAddedTrackModel.queueItems(),
                                    model.trackId,
                                    model.filePath)
                            }
                        }
                    }
                }

                GlassPanel {
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    radius: Theme.radiusLarge

                    Text {
                        id: recentPlayedHeading
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.top: parent.top
                        anchors.topMargin: 16
                        text: (I18n.revision, I18n.t("library.recentPlayed"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.centerIn: recentPlayedList
                        width: recentPlayedList.width - 24
                        text: (I18n.revision, I18n.t("empty.recentPlayed.title"))
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: RecentPlayedTrackModel.count === 0
                    }

                    ListView {
                        id: recentPlayedList
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: recentPlayedHeading.bottom
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        anchors.topMargin: 10
                        clip: true
                        model: RecentPlayedTrackModel
                        visible: RecentPlayedTrackModel.count > 0

                        delegate: Rectangle {
                            id: recentPlayedDelegate

                            property int rowIndex: index

                            width: recentPlayedList.width
                            height: 52
                            radius: Theme.radiusSmall
                            color: recentPlayedMouse.containsMouse ? Theme.accentSoft : "transparent"

                            Text {
                                id: recentPlayedTitle
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: recentPlayedDuration.left
                                anchors.rightMargin: 12
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                text: model.title
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                anchors.left: recentPlayedTitle.left
                                anchors.right: recentPlayedTitle.right
                                anchors.top: recentPlayedTitle.bottom
                                anchors.topMargin: 3
                                text: model.artist && model.artist.length > 0 ? model.artist : (I18n.revision, I18n.t("metadata.unknownArtist"))
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Text {
                                id: recentPlayedDuration
                                anchors.right: recentPlayedQueue.left
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: 54
                                text: root.formatDuration(model.durationMs)
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                            }

                            AddToQueueButton {
                                id: recentPlayedQueue
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                filePath: model.filePath
                                trackId: model.trackId
                                title: model.title
                                artist: model.artist
                                album: model.album
                                coverUrl: model.coverUrl
                            }

                            MouseArea {
                                id: recentPlayedMouse
                                anchors.fill: parent
                                anchors.rightMargin: 42
                                hoverEnabled: true
                                onDoubleClicked: PlaybackController.playQueueItemByIdentity(
                                    RecentPlayedTrackModel.queueItems(),
                                    model.trackId,
                                    model.filePath)
                            }
                        }
                    }
                }
            }

            GlassPanel {
                x: root.pageMargin
                width: parent.width - root.pageMargin * 2
                height: 248
                radius: Theme.radiusLarge

                Text {
                    id: duplicatesHeading
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    text: (I18n.revision, I18n.t("library.duplicates"))
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    id: duplicatesCount
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: duplicatesHeading.verticalCenter
                    text: DuplicateTrackModel.count + " " + (I18n.revision, I18n.t("detail.tracks"))
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    visible: DuplicateTrackModel.count > 0
                }

                Text {
                    anchors.centerIn: duplicateList
                    width: duplicateList.width - 24
                    text: (I18n.revision, I18n.t("empty.duplicates.title"))
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: DuplicateTrackModel.count === 0
                }

                ListView {
                    id: duplicateList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: duplicatesHeading.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    anchors.topMargin: 10
                    clip: true
                    model: DuplicateTrackModel
                    visible: DuplicateTrackModel.count > 0

                    delegate: Rectangle {
                        id: duplicateDelegate

                        property int rowIndex: index

                        width: duplicateList.width
                        height: 52
                        radius: Theme.radiusSmall
                        color: duplicateMouse.containsMouse ? Theme.accentSoft : "transparent"

                        Text {
                            id: duplicateTitle
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.right: duplicateQueue.left
                            anchors.rightMargin: 12
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            text: model.title
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            anchors.left: duplicateTitle.left
                            anchors.right: duplicateTitle.right
                            anchors.top: duplicateTitle.bottom
                            anchors.topMargin: 3
                            text: model.filePath
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }

                        AddToQueueButton {
                            id: duplicateQueue
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            filePath: model.filePath
                            trackId: model.trackId
                            title: model.title
                            artist: model.artist
                            album: model.album
                            coverUrl: model.coverUrl
                        }

                        MouseArea {
                            id: duplicateMouse
                            anchors.fill: parent
                            anchors.rightMargin: 42
                            hoverEnabled: true
                            onDoubleClicked: PlaybackController.playQueueItemByIdentity(
                                DuplicateTrackModel.queueItems(),
                                model.trackId,
                                model.filePath)
                        }
                    }
                }
            }

            Item {
                width: 1
                height: 10
            }
        }
    }
}
