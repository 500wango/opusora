import QtQuick
import QtQuick.Controls
import Opusora

IconButton {
    id: root

    property int trackId: 0

    width: 32
    height: 32
    iconName: "playlist"
    enabled: root.trackId > 0 && PlaylistModel.count > 0

    tooltipText: (I18n.revision, I18n.t("action.addToPlaylist"))

    onClicked: playlistMenu.open()

    Menu {
        id: playlistMenu

        Repeater {
            model: PlaylistModel

            delegate: MenuItem {
                text: model.name
                onTriggered: PlaylistModel.addTrack(model.playlistId, root.trackId)
            }
        }
    }
}
