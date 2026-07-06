import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Opusora

Item {
    id: root

    signal searchRequested(string query)
    signal settingsRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.mode === "dark" ? Qt.rgba(0.08, 0.09, 0.13, 0.42) : Qt.rgba(1.0, 1.0, 1.0, 0.34)

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.glassHighlight
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.hairline
        }
    }

    DragHandler {
        target: null
        acceptedButtons: Qt.LeftButton
        onActiveChanged: {
            if (active && root.Window.window) {
                root.Window.window.startSystemMove()
            }
        }
    }

    Row {
        id: trafficButtons
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        WindowTrafficButton {
            buttonColor: "#ff5f57"
            onClicked: Qt.quit()
        }

        WindowTrafficButton {
            buttonColor: "#ffbd2e"
            onClicked: if (root.Window.window) root.Window.window.showMinimized()
        }

        WindowTrafficButton {
            buttonColor: "#28c840"
            onClicked: {
                if (root.Window.window) {
                    if (root.Window.window.visibility === Window.Maximized) {
                        root.Window.window.showNormal()
                    } else {
                        root.Window.window.showMaximized()
                    }
                }
            }
        }
    }

    Text {
        anchors.left: trafficButtons.right
        anchors.leftMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        text: (I18n.revision, I18n.t("app.title"))
        color: Theme.textPrimary
        font.pixelSize: 13
        font.weight: Font.DemiBold
        opacity: 0.92
    }

    IconButton {
        id: settingsButton
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        iconName: "settings"
        tooltipText: (I18n.revision, I18n.t("nav.settings"))
        onClicked: root.settingsRequested()
    }

    SearchBox {
        anchors.right: settingsButton.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        onSearchRequested: function(query) {
            root.searchRequested(query)
        }
    }
}
