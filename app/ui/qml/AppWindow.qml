import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Opusora

ApplicationWindow {
    id: root

    width: SettingsController.windowWidth
    height: SettingsController.windowHeight
    minimumWidth: 920
    minimumHeight: 620
    visible: true
    title: (I18n.revision, I18n.t("app.title"))
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    property string currentPage: "library"
    readonly property bool showingWelcome: !SettingsController.welcomeCompleted && LibraryController.roots.length === 0
    readonly property int textRevision: I18n.revision

    onClosing: SettingsController.setWindowSize(width, height)

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

    function applyOnlineMetadata(trackId, title, artist, album) {
        if (trackId <= 0) {
            return
        }

        const current = PlaybackController.currentTrackId === trackId
        if (current) {
            const nextTitle = title && title.trim().length > 0
                ? title.trim()
                : PlaybackController.title
            const nextArtist = artist && artist.trim().length > 0
                ? artist.trim()
                : PlaybackController.artist
            const nextAlbum = album && album.trim().length > 0
                ? album.trim()
                : PlaybackController.album
            PlaybackController.updateCurrentTrackDisplay(nextTitle, nextArtist, nextAlbum)
        }
        root.refreshLibraryViews()
    }

    function applyOnlineLyrics(trackId, lyrics) {
        if (trackId <= 0 || PlaybackController.currentTrackId !== trackId) {
            return
        }

        const title = PlaybackController.title
        if (title.trim().length === 0) {
            return
        }

        if (TrackModel.saveTrackDetails(
                trackId,
                title,
                PlaybackController.artist,
                PlaybackController.album,
                lyrics)) {
            root.refreshLibraryViews()
        }
    }

    function pageIndex(page) {
        switch (page) {
        case "library":
            return 0
        case "songs":
            return 1
        case "albums":
            return 2
        case "artists":
            return 3
        case "genres":
            return 4
        case "playlists":
            return 5
        case "queue":
            return 6
        case "nowPlaying":
            return 7
        case "radio":
            return 8
        case "search":
            return 9
        case "settings":
            return 10
        default:
            return 0
        }
    }

    Connections {
        target: OnlineMetadataService

        function onLyricsFetched(trackId, lyrics, title, artist, album) {
            root.applyOnlineLyrics(trackId, lyrics)
        }

        function onMetadataFetched(trackId, title, artist, album) {
            root.applyOnlineMetadata(trackId, title, artist, album)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.visibility === Window.Maximized ? 0 : 18
        clip: true
        border.color: root.visibility === Window.Maximized ? "transparent" : Theme.glassBorder
        border.width: root.visibility === Window.Maximized ? 0 : 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.backgroundTop }
            GradientStop { position: 1.0; color: Theme.backgroundBottom }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height * 0.72
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.backgroundSheen }
                GradientStop {
                    position: 1.0
                    color: Theme.mode === "dark"
                        ? Qt.rgba(0.42, 0.55, 0.74, 0.0)
                        : Qt.rgba(1.0, 1.0, 1.0, 0.0)
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.glassHighlight
        }

        MacTitleBar {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.titleBarHeight

            onSearchRequested: function(query) {
                root.currentPage = "search"
                SearchController.search(query)
            }

            onSettingsRequested: {
                root.currentPage = "settings"
            }
        }

        Sidebar {
            id: sidebar
            anchors.top: titleBar.bottom
            anchors.topMargin: 8
            anchors.bottom: playerBar.top
            anchors.bottomMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 12
            width: root.showingWelcome ? 0 : Theme.sidebarWidth
            visible: !root.showingWelcome
            currentPage: root.currentPage

            onNavigate: function(page) {
                root.currentPage = page
            }
        }

        Loader {
            id: contentLoader
            anchors.top: titleBar.bottom
            anchors.topMargin: root.showingWelcome ? 0 : 4
            anchors.bottom: playerBar.top
            anchors.bottomMargin: root.showingWelcome ? 0 : 8
            anchors.left: root.showingWelcome ? parent.left : sidebar.right
            anchors.leftMargin: root.showingWelcome ? 0 : 12
            anchors.right: parent.right
            anchors.rightMargin: root.showingWelcome ? 0 : 14
            sourceComponent: {
                if (root.showingWelcome) {
                    return welcomePage
                }
                switch (root.currentPage) {
                case "songs":
                    return songsPage
                case "albums":
                    return albumsPage
                case "artists":
                    return artistsPage
                case "genres":
                    return genresPage
                case "playlists":
                    return playlistsPage
                case "queue":
                    return queuePage
                case "nowPlaying":
                    return nowPlayingPage
                case "radio":
                    return radioPage
                case "search":
                    return searchResultsPage
                case "settings":
                    return settingsPage
                default:
                    return libraryPage
                }
            }
        }

        PlayerBar {
            id: playerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.showingWelcome ? 0 : Theme.playerBarHeight
            visible: !root.showingWelcome
            trackTitle: PlaybackController.title
            artistName: PlaybackController.artist
            coverUrl: PlaybackController.coverUrl
            positionMs: PlaybackController.positionMs
            durationMs: PlaybackController.durationMs
            playbackState: PlaybackController.playbackState
            errorMessage: PlaybackController.errorMessage
            isPlaying: PlaybackController.isPlaying
            volume: PlaybackController.volume
            muted: PlaybackController.muted
            playbackMode: PlaybackController.playbackMode

            onPlayPauseClicked: PlaybackController.togglePlayPause()
            onOpenFileRequested: PlaybackController.requestOpenFile()
            onPreviousClicked: PlaybackController.previous()
            onNextClicked: PlaybackController.next()
            onStopClicked: PlaybackController.stop()
            onMuteClicked: PlaybackController.toggleMuted()
            onPlaybackModeClicked: PlaybackController.cyclePlaybackMode()
            onSeekRequested: function(ms) { PlaybackController.seek(ms) }
            onVolumeRequested: function(value) { PlaybackController.setVolume(value) }
        }

        Component { id: welcomePage; WelcomePage {} }
        Component { id: libraryPage; LibraryPage {} }
        Component { id: songsPage; SongsPage {} }
        Component { id: albumsPage; AlbumsPage {} }
        Component { id: artistsPage; ArtistsPage {} }
        Component { id: genresPage; GenresPage {} }
        Component { id: playlistsPage; PlaylistsPage {} }
        Component { id: queuePage; QueuePage {} }
        Component { id: nowPlayingPage; NowPlayingPage {} }
        Component { id: radioPage; RadioPage {} }
        Component { id: searchResultsPage; SearchResultsPage {} }
        Component { id: settingsPage; SettingsPage {} }
    }
}
