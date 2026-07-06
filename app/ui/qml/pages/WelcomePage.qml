import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Opusora

Item {
    id: root

    function finishWelcome() {
        SettingsController.setWelcomeCompleted(true)
    }

    Connections {
        target: LibraryController

        function onRootsChanged() {
            if (LibraryController.roots.length > 0) {
                root.finishWelcome()
            }
        }
    }

    GlassPanel {
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, 540)
        height: welcomeContent.implicitHeight + 52
        radius: Theme.radiusLarge

        Column {
            id: welcomeContent
            anchors.fill: parent
            anchors.margins: 26
            spacing: 18

            Text {
                text: (I18n.revision, I18n.t("welcome.title"))
                color: Theme.textPrimary
                font.pixelSize: 34
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Text {
                text: (I18n.revision, I18n.t("welcome.subtitle"))
                color: Theme.textSecondary
                font.pixelSize: 15
                lineHeight: 1.25
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Column {
                width: parent.width
                spacing: 8

                Text {
                    text: (I18n.revision, I18n.t("settings.language"))
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    width: parent.width
                }

                GlassComboBox {
                    width: parent.width
                    model: [
                        { label: "English", value: "en-US" },
                        { label: "简体中文", value: "zh-CN" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: SettingsController.language === "zh-CN" ? 1 : 0
                    onActivated: SettingsController.setLanguage(currentValue)
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                GlassButton {
                    width: 118
                    text: (I18n.revision, I18n.t("action.skip"))
                    onClicked: root.finishWelcome()
                }

                GlassButton {
                    width: 150
                    text: (I18n.revision, I18n.t("action.addFolder"))
                    highlighted: true
                    onClicked: LibraryController.requestAddFolder()
                }
            }
        }
    }
}
