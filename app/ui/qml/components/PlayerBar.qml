import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property string trackTitle
    property string artistName
    property url coverUrl
    property int positionMs
    property int durationMs
    property string playbackState
    property string errorMessage
    property bool isPlaying
    property real volume
    property bool muted
    property string playbackMode

    signal playPauseClicked()
    signal openFileRequested()
    signal previousClicked()
    signal nextClicked()
    signal stopClicked()
    signal muteClicked()
    signal playbackModeClicked()
    signal seekRequested(int positionMs)
    signal volumeRequested(real value)

    function playbackModeText(mode) {
        switch (mode) {
        case "repeatOne":
            return (I18n.revision, I18n.t("playback.mode.repeatOne"))
        case "repeatAll":
            return (I18n.revision, I18n.t("playback.mode.repeatAll"))
        case "shuffle":
            return (I18n.revision, I18n.t("playback.mode.shuffle"))
        default:
            return (I18n.revision, I18n.t("playback.mode.sequential"))
        }
    }

    function formatTime(ms) {
        if (!ms || ms <= 0) {
            return "0:00"
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

    Rectangle {
        id: shadow
        anchors.fill: panel
        anchors.topMargin: 12
        anchors.leftMargin: 3
        anchors.rightMargin: 3
        radius: panel.radius
        color: Theme.shadow
        opacity: 0.50
    }

    Rectangle {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 4
        anchors.bottomMargin: 12
        radius: 26
        color: Theme.playerBarBackground
        border.color: Theme.playerControlStroke
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height * 0.42
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: 0.20
        }
    }

    Item {
        id: timeline
        anchors.left: panel.left
        anchors.right: panel.right
        anchors.top: panel.top
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 9
        height: 28

        Text {
            id: elapsedText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 52
            text: root.formatTime(root.positionMs)
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }

        ProgressSlider {
            id: progress
            anchors.left: elapsedText.right
            anchors.right: durationText.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 28
            from: 0
            to: Math.max(root.durationMs, 1)
            value: Math.min(root.positionMs, Math.max(root.durationMs, 1))
            enabled: root.durationMs > 0
            opacity: enabled ? 1.0 : 0.62
            trackHeight: 7
            handleSize: 16
            onMoved: root.seekRequested(Math.round(value))
        }

        Text {
            id: durationText
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 58
            text: root.durationMs > 0 ? root.formatTime(root.durationMs) : "--:--"
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    CoverImage {
        id: cover
        anchors.left: panel.left
        anchors.leftMargin: 18
        anchors.verticalCenter: panel.verticalCenter
        anchors.verticalCenterOffset: 13
        width: 58
        height: 58
        source: root.coverUrl
        radius: 14
    }

    Item {
        id: trackInfo
        anchors.left: cover.right
        anchors.leftMargin: 14
        anchors.verticalCenter: cover.verticalCenter
        width: 214
        height: 46

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            text: root.trackTitle.length > 0 ? root.trackTitle : (I18n.revision, I18n.t("player.noTrack"))
            color: Theme.textPrimary
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            text: root.errorMessage.length > 0
                ? root.errorMessage
                : (root.artistName.length > 0
                    ? root.artistName
                    : ((I18n.revision, I18n.t("player.state")) + ": " + root.playbackState))
            color: Theme.textSecondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: controls
        anchors.left: trackInfo.right
        anchors.leftMargin: 22
        anchors.verticalCenter: cover.verticalCenter
        width: controlsRow.implicitWidth + 28
        height: 52
        radius: height / 2
        color: Theme.playerControlGroup
        border.color: Theme.playerControlStroke
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: Theme.mode === "dark" ? 0.05 : 0.16
        }

        Row {
            id: controlsRow
            anchors.centerIn: parent
            spacing: 8

            IconButton {
                iconName: "open"
                quiet: true
                tooltipText: (I18n.revision, I18n.t("action.openFile"))
                onClicked: root.openFileRequested()
            }

            Rectangle {
                width: 1
                height: 22
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.hairline
                opacity: 0.65
            }

            IconButton {
                iconName: "previous"
                quiet: true
                tooltipText: "Previous"
                onClicked: root.previousClicked()
            }

            IconButton {
                iconName: root.isPlaying ? "pause" : "play"
                emphasized: true
                tooltipText: root.isPlaying ? "Pause" : "Play"
                onClicked: root.playPauseClicked()
            }

            IconButton {
                iconName: "next"
                quiet: true
                tooltipText: "Next"
                onClicked: root.nextClicked()
            }

            IconButton {
                iconName: "stop"
                quiet: true
                tooltipText: (I18n.revision, I18n.t("action.stop"))
                enabled: root.trackTitle.length > 0
                onClicked: root.stopClicked()
            }

            IconButton {
                iconName: root.playbackMode
                active: root.playbackMode !== "sequential"
                quiet: true
                tooltipText: root.playbackModeText(root.playbackMode)
                onClicked: root.playbackModeClicked()
            }
        }
    }

    Rectangle {
        id: volumeControls
        anchors.right: panel.right
        anchors.rightMargin: 18
        anchors.verticalCenter: cover.verticalCenter
        width: volumeRow.implicitWidth + 18
        height: 46
        radius: height / 2
        color: Theme.playerControlGroup
        border.color: Theme.playerControlStroke
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: Theme.mode === "dark" ? 0.04 : 0.14
        }

        Row {
            id: volumeRow
            anchors.centerIn: parent
            spacing: 10

            IconButton {
                iconName: root.muted || root.volume <= 0 ? "mute" : "volume"
                quiet: true
                tooltipText: root.muted
                    ? (I18n.revision, I18n.t("action.unmute"))
                    : (I18n.revision, I18n.t("action.mute"))
                onClicked: root.muteClicked()
            }

            ProgressSlider {
                id: volumeSlider
                width: 92
                height: 34
                from: 0
                to: 1
                value: root.volume
                trackHeight: 5
                handleSize: 14
                onMoved: root.volumeRequested(value)
            }
        }
    }
}
