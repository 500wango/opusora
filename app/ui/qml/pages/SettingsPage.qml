import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Opusora

Item {
    id: root

    property var themeOptions: [
        { label: (I18n.revision, I18n.t("theme.light")), value: "light" },
        { label: (I18n.revision, I18n.t("theme.dark")), value: "dark" },
        { label: (I18n.revision, I18n.t("theme.system")), value: "system" }
    ]
    property var sortOptions: [
        { label: (I18n.revision, I18n.t("sort.title")), value: "title" },
        { label: (I18n.revision, I18n.t("sort.artist")), value: "artist" },
        { label: (I18n.revision, I18n.t("sort.album")), value: "album" },
        { label: (I18n.revision, I18n.t("sort.recent")), value: "recent" },
        { label: (I18n.revision, I18n.t("sort.duration")), value: "duration" }
    ]

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: root.width
            spacing: 18

            Text {
                text: (I18n.revision, I18n.t("page.settings"))
                color: Theme.textPrimary
                font.pixelSize: 30
                font.weight: Font.DemiBold
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 24
            }

            GlassPanel {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.fillWidth: true
                Layout.preferredHeight: languageContent.implicitHeight + 36
                radius: Theme.radiusLarge

                ColumnLayout {
                    id: languageContent
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: (I18n.revision, I18n.t("settings.language"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    GlassComboBox {
                        objectName: "settingsLanguageComboBox"
                        Layout.preferredWidth: 220
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
            }

            GlassPanel {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.fillWidth: true
                Layout.preferredHeight: themeContent.implicitHeight + 36
                radius: Theme.radiusLarge

                ColumnLayout {
                    id: themeContent
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: (I18n.revision, I18n.t("settings.theme"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    GlassComboBox {
                        objectName: "settingsThemeComboBox"
                        Layout.preferredWidth: 220
                        model: root.themeOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: SettingsController.themeMode === "dark"
                            ? 1
                            : (SettingsController.themeMode === "system" ? 2 : 0)
                        onActivated: SettingsController.setThemeMode(currentValue)
                    }
                }
            }

            GlassPanel {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.fillWidth: true
                Layout.preferredHeight: playbackContent.implicitHeight + 36
                radius: Theme.radiusLarge

                ColumnLayout {
                    id: playbackContent
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16

                    Text {
                        text: (I18n.revision, I18n.t("settings.playback"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.volume"))
                            color: Theme.textPrimary
                            Layout.preferredWidth: 150
                        }

                        ProgressSlider {
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: SettingsController.volume
                            onMoved: SettingsController.setVolume(value)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.restoreQueue"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassSwitch {
                            checked: SettingsController.restoreQueueOnStartup
                            onToggled: SettingsController.setRestoreQueueOnStartup(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.notifications"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassSwitch {
                            checked: SettingsController.desktopNotificationsEnabled
                            onToggled: SettingsController.setDesktopNotificationsEnabled(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.mediaKeys"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassSwitch {
                            checked: SettingsController.mediaKeysEnabled
                            onToggled: SettingsController.setMediaKeysEnabled(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.replayGain"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassSwitch {
                            checked: SettingsController.replayGainEnabled
                            onToggled: SettingsController.setReplayGainEnabled(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.gaplessPlayback"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassSwitch {
                            checked: SettingsController.gaplessPlaybackEnabled
                            onToggled: SettingsController.setGaplessPlaybackEnabled(checked)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.defaultSortOrder"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassComboBox {
                            Layout.preferredWidth: 190
                            model: root.sortOptions
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: {
                                const values = ["title", "artist", "album", "recent", "duration"]
                                const index = values.indexOf(SettingsController.defaultSortOrder)
                                return index >= 0 ? index : 0
                            }
                            onActivated: SettingsController.setDefaultSortOrder(currentValue)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.hairline
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("settings.equalizer"))
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GlassButton {
                            text: (I18n.revision, I18n.t("action.reset"))
                            onClicked: PlaybackController.resetEqualizer()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("equalizer.low"))
                            color: Theme.textSecondary
                            Layout.preferredWidth: 80
                        }

                        ProgressSlider {
                            Layout.fillWidth: true
                            from: -12
                            to: 12
                            value: PlaybackController.equalizerLowGain
                            onMoved: PlaybackController.setEqualizerLowGain(value)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("equalizer.mid"))
                            color: Theme.textSecondary
                            Layout.preferredWidth: 80
                        }

                        ProgressSlider {
                            Layout.fillWidth: true
                            from: -12
                            to: 12
                            value: PlaybackController.equalizerMidGain
                            onMoved: PlaybackController.setEqualizerMidGain(value)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Text {
                            text: (I18n.revision, I18n.t("equalizer.high"))
                            color: Theme.textSecondary
                            Layout.preferredWidth: 80
                        }

                        ProgressSlider {
                            Layout.fillWidth: true
                            from: -12
                            to: 12
                            value: PlaybackController.equalizerHighGain
                            onMoved: PlaybackController.setEqualizerHighGain(value)
                        }
                    }
                }
            }

            GlassPanel {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.fillWidth: true
                Layout.preferredHeight: libraryContent.implicitHeight + 36
                Layout.bottomMargin: 28
                radius: Theme.radiusLarge
                visible: true

                ColumnLayout {
                    id: libraryContent
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: (I18n.revision, I18n.t("settings.libraryFolders"))
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: LibraryController.roots

                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                text: modelData
                                color: Theme.textPrimary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            GlassButton {
                                text: (I18n.revision, I18n.t("action.remove"))
                                destructive: true
                                onClicked: LibraryController.removeRoot(modelData)
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: (I18n.revision, I18n.t("empty.library.title"))
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        visible: LibraryController.roots.length === 0
                    }

                    GlassButton {
                        text: (I18n.revision, I18n.t("action.addFolder"))
                        emphasized: true
                        onClicked: LibraryController.requestAddFolder()
                    }
                }
            }
        }
    }
}
