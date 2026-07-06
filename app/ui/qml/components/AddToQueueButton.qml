import QtQuick.Controls
import Opusora

IconButton {
    id: root

    property string filePath: ""
    property int trackId: 0
    property string title: ""
    property string artist: ""
    property string album: ""
    property string coverUrl: ""

    iconName: "queue"
    tooltipText: (I18n.revision, I18n.t("action.addToQueue"))
    enabled: root.filePath.length > 0

    onClicked: PlaybackController.appendQueueItem({
        "trackId": root.trackId,
        "filePath": root.filePath,
        "title": root.title,
        "artist": root.artist,
        "album": root.album,
        "coverUrl": root.coverUrl
    })
}
