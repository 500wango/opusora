import QtQuick
import QtQuick.Controls
import Opusora

Item {
    id: root

    property string stationNameDraft: ""
    property string stationUrlDraft: ""

    function playStation(name, url) {
        PlaybackController.openStream(url, name)
    }

    Text {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        text: (I18n.revision, I18n.t("page.radio"))
        color: Theme.textPrimary
        font.pixelSize: 30
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    GlassPanel {
        id: addPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        height: 74
        radius: Theme.radiusLarge

        GlassTextField {
            id: nameField
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(160, parent.width * 0.22)
            placeholderText: (I18n.revision, I18n.t("radio.name"))
            text: root.stationNameDraft
            onTextChanged: root.stationNameDraft = text
        }

        GlassTextField {
            id: urlField
            anchors.left: nameField.right
            anchors.leftMargin: 10
            anchors.right: addButton.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: (I18n.revision, I18n.t("radio.url"))
            text: root.stationUrlDraft
            onTextChanged: root.stationUrlDraft = text
        }

        GlassButton {
            id: addButton
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 104
            height: 34
            text: (I18n.revision, I18n.t("action.add"))
            enabled: root.stationUrlDraft.trim().length > 0
            onClicked: {
                if (RadioModel.addStation(root.stationNameDraft, root.stationUrlDraft)) {
                    root.stationNameDraft = ""
                    root.stationUrlDraft = ""
                    nameField.text = ""
                    urlField.text = ""
                }
            }
        }
    }

    EmptyState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: addPanel.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        title: (I18n.revision, I18n.t("empty.radio.title"))
        actionText: ""
        visible: RadioModel.count === 0
    }

    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: addPanel.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 24
        clip: true
        model: RadioModel
        visible: RadioModel.count > 0
        spacing: 1

        delegate: Rectangle {
            id: stationDelegate

            width: listView.width
            height: 56
            radius: Theme.radiusSmall
            color: mouseArea.containsMouse ? Theme.accentSoft : "transparent"

            Text {
                id: nameText
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: actions.left
                anchors.rightMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 8
                text: model.name
                color: Theme.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                anchors.left: nameText.left
                anchors.right: nameText.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                text: model.url
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onDoubleClicked: root.playStation(model.name, model.url)
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
                    onClicked: root.playStation(model.name, model.url)
                }

                IconButton {
                    iconName: "remove"
                    tooltipText: (I18n.revision, I18n.t("action.remove"))
                    onClicked: RadioModel.removeStation(index)
                }
            }
        }
    }
}
