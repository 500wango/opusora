pragma Singleton

import QtQuick

QtObject {
    id: theme

    readonly property string mode: SettingsController.themeMode === "dark"
        || (SettingsController.themeMode === "system" && SettingsController.systemThemeMode === "dark")
        ? "dark"
        : "light"

    readonly property int sidebarWidth: 258
    readonly property int titleBarHeight: 64
    readonly property int playerBarHeight: 108

    readonly property int radiusSmall: 10
    readonly property int radiusMedium: 16
    readonly property int radiusLarge: 24

    readonly property color background: mode === "dark" ? "#11131a" : "#f4f7fb"
    readonly property color backgroundTop: mode === "dark" ? "#1a1d29" : "#fbfdff"
    readonly property color backgroundBottom: mode === "dark" ? "#090a0f" : "#e9eef6"
    readonly property color backgroundSheen: mode === "dark" ? Qt.rgba(0.42, 0.55, 0.74, 0.10) : Qt.rgba(1.0, 1.0, 1.0, 0.58)

    readonly property color surface: mode === "dark" ? Qt.rgba(0.11, 0.12, 0.17, 0.72) : Qt.rgba(1.0, 1.0, 1.0, 0.62)
    readonly property color elevated: mode === "dark" ? Qt.rgba(0.16, 0.17, 0.23, 0.82) : Qt.rgba(1.0, 1.0, 1.0, 0.78)
    readonly property color sidebarBackground: mode === "dark" ? Qt.rgba(0.12, 0.13, 0.18, 0.66) : Qt.rgba(0.96, 0.98, 1.0, 0.62)
    readonly property color playerBarBackground: mode === "dark" ? Qt.rgba(0.10, 0.105, 0.12, 0.84) : Qt.rgba(0.98, 0.99, 1.0, 0.80)
    readonly property color playerControlGroup: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.07) : Qt.rgba(0.08, 0.10, 0.13, 0.06)
    readonly property color playerControlStroke: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.10) : Qt.rgba(0.20, 0.24, 0.30, 0.10)
    readonly property color playerPrimaryControl: mode === "dark" ? Qt.rgba(0.96, 0.97, 0.99, 0.94) : Qt.rgba(0.10, 0.11, 0.13, 0.92)
    readonly property color playerPrimaryControlPressed: mode === "dark" ? Qt.rgba(0.82, 0.84, 0.88, 0.94) : Qt.rgba(0.02, 0.025, 0.035, 0.96)
    readonly property color playerPrimaryIcon: mode === "dark" ? "#14161a" : "#ffffff"
    readonly property color controlFill: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.07) : Qt.rgba(1.0, 1.0, 1.0, 0.50)
    readonly property color controlHover: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.13) : Qt.rgba(1.0, 1.0, 1.0, 0.84)
    readonly property color controlPressed: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.18) : Qt.rgba(0.08, 0.10, 0.13, 0.10)
    readonly property color popupFill: mode === "dark" ? "#252936" : "#ffffff"
    readonly property color popupItemFill: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.08) : Qt.rgba(0.08, 0.10, 0.13, 0.06)
    readonly property color popupItemHover: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.12) : Qt.rgba(0.08, 0.10, 0.13, 0.09)
    readonly property color glassHighlight: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.08) : Qt.rgba(1.0, 1.0, 1.0, 0.90)
    readonly property color glassBorder: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.14) : Qt.rgba(1.0, 1.0, 1.0, 0.88)
    readonly property color innerStroke: mode === "dark" ? Qt.rgba(0.0, 0.0, 0.0, 0.34) : Qt.rgba(0.43, 0.48, 0.56, 0.18)
    readonly property color shadow: mode === "dark" ? Qt.rgba(0.0, 0.0, 0.0, 0.42) : Qt.rgba(0.17, 0.22, 0.30, 0.18)

    readonly property color textPrimary: mode === "dark" ? "#f7f8fb" : "#15171d"
    readonly property color textSecondary: mode === "dark" ? "#aeb4c1" : "#5f6673"
    readonly property color hairline: mode === "dark" ? Qt.rgba(1.0, 1.0, 1.0, 0.12) : Qt.rgba(0.36, 0.43, 0.54, 0.18)
    readonly property color accent: mode === "dark" ? "#6ea8ff" : "#0a84ff"
    readonly property color accentStrong: mode === "dark" ? "#8ab4ff" : "#0067d8"
    readonly property color accentSoft: mode === "dark" ? Qt.rgba(0.35, 0.55, 1.0, 0.24) : Qt.rgba(0.04, 0.52, 1.0, 0.16)
}
