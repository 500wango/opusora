import QtQuick
import Opusora

Item {
    id: root

    property string currentPage
    signal navigate(string page)

    Rectangle {
        id: shadow
        anchors.fill: panel
        anchors.topMargin: 10
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        radius: panel.radius
        color: Theme.shadow
        opacity: 0.42
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.sidebarBackground
        border.color: Theme.glassBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height * 0.34
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: 0.22
        }
    }

    Column {
        anchors.fill: panel
        anchors.margins: 14
        spacing: 7

        Text {
            width: parent.width
            height: 28
            text: (I18n.revision, I18n.t("app.title")).toUpperCase()
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            opacity: 0.78
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.library"))
            selected: root.currentPage === "library"
            onClicked: root.navigate("library")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.songs"))
            selected: root.currentPage === "songs"
            onClicked: root.navigate("songs")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.albums"))
            selected: root.currentPage === "albums"
            onClicked: root.navigate("albums")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.artists"))
            selected: root.currentPage === "artists"
            onClicked: root.navigate("artists")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.genres"))
            selected: root.currentPage === "genres"
            onClicked: root.navigate("genres")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.playlists"))
            selected: root.currentPage === "playlists"
            onClicked: root.navigate("playlists")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.queue")) + (PlaybackController.queueCount > 0 ? " (" + PlaybackController.queueCount + ")" : "")
            selected: root.currentPage === "queue"
            onClicked: root.navigate("queue")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.nowPlaying"))
            selected: root.currentPage === "nowPlaying"
            onClicked: root.navigate("nowPlaying")
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.radio"))
            selected: root.currentPage === "radio"
            onClicked: root.navigate("radio")
        }

        Item {
            width: parent.width
            height: Math.max(0, root.height - 14 * 2 - 28 - 7 * 11 - 40 * 10)
        }

        SidebarItem {
            width: parent.width
            text: (I18n.revision, I18n.t("nav.settings"))
            selected: root.currentPage === "settings"
            onClicked: root.navigate("settings")
        }
    }
}
