import QtQuick
import QtQuick.Controls
import Opusora

Control {
    id: root

    property var model: []
    property string textRole: ""
    property string valueRole: ""
    property int currentIndex: -1
    readonly property string displayText: optionText(currentIndex)
    readonly property var currentValue: optionValue(currentIndex)
    property int highlightedIndex: currentIndex
    property bool pressed: triggerMouse.pressed || popup.visible

    signal activated(int index)

    implicitHeight: 36
    implicitWidth: 180
    leftPadding: 14
    rightPadding: 34
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 13

    function optionCount() {
        if (!model) {
            return 0
        }
        if (model.length !== undefined) {
            return model.length
        }
        if (model.count !== undefined) {
            return model.count
        }
        return 0
    }

    function optionAt(index) {
        if (!model || index < 0 || index >= optionCount()) {
            return undefined
        }
        if (model.get && typeof model.get === "function") {
            return model.get(index)
        }
        return model[index]
    }

    function roleValue(option, role) {
        if (option === undefined || option === null) {
            return ""
        }
        if (role.length > 0 && typeof option === "object") {
            const value = option[role]
            return value === undefined || value === null ? "" : value
        }
        return option
    }

    function optionText(index) {
        return String(roleValue(optionAt(index), textRole))
    }

    function optionValue(index) {
        return roleValue(optionAt(index), valueRole)
    }

    function updatePopupGeometry() {
        const overlay = Overlay.overlay
        if (!overlay) {
            popup.x = 0
            popup.y = height + 6
            popup.width = width
            return
        }

        const popupPosition = mapToItem(overlay, 0, height + 6)
        popup.x = popupPosition.x
        popup.y = popupPosition.y
        popup.width = width
    }

    function openPopup() {
        if (optionCount() <= 0) {
            return
        }

        forceActiveFocus()
        highlightedIndex = currentIndex >= 0 ? currentIndex : 0
        updatePopupGeometry()
        popup.open()
    }

    function activateOption(index) {
        if (index < 0 || index >= optionCount()) {
            return
        }

        currentIndex = index
        highlightedIndex = index
        popup.close()
        activated(index)
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (popup.visible) {
                activateOption(highlightedIndex)
            } else {
                openPopup()
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && popup.visible) {
            popup.close()
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (!popup.visible) {
                openPopup()
            } else {
                highlightedIndex = Math.min(optionCount() - 1, highlightedIndex + 1)
                optionsView.positionViewAtIndex(highlightedIndex, ListView.Contain)
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (!popup.visible) {
                openPopup()
            } else {
                highlightedIndex = Math.max(0, highlightedIndex - 1)
                optionsView.positionViewAtIndex(highlightedIndex, ListView.Contain)
            }
            event.accepted = true
        }
    }

    contentItem: Item {
        implicitWidth: displayLabel.implicitWidth + 48
        implicitHeight: 36

        Text {
            id: displayLabel
            anchors.left: parent.left
            anchors.leftMargin: root.leftPadding
            anchors.right: chevron.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.displayText
            color: root.enabled ? Theme.textPrimary : Theme.textSecondary
            opacity: root.enabled ? 1.0 : 0.62
            font.pixelSize: root.font.pixelSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: 13
            anchors.verticalCenter: parent.verticalCenter
            text: "⌄"
            color: Theme.textSecondary
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }

    background: Rectangle {
        radius: height / 2
        color: root.pressed || root.hovered ? Theme.controlHover : Theme.controlFill
        border.color: root.activeFocus ? Theme.accent : Theme.glassBorder
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: parent.height / 2
            radius: parent.radius
            color: Theme.glassHighlight
            opacity: 0.26
        }
    }

    MouseArea {
        id: triggerMouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: popup.visible ? popup.close() : root.openPopup()
    }

    Popup {
        id: popup

        parent: Overlay.overlay
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 6
        height: Math.min(optionsView.contentHeight + topPadding + bottomPadding, 268)

        onOpened: {
            root.updatePopupGeometry()
            optionsView.positionViewAtIndex(root.highlightedIndex, ListView.Contain)
        }

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.popupFill
            border.color: Theme.glassBorder
            border.width: 1

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Math.max(0, parent.radius - 1)
                color: "transparent"
                border.color: Theme.innerStroke
                border.width: 1
            }
        }

        contentItem: ListView {
            id: optionsView

            clip: true
            implicitHeight: Math.min(contentHeight, 256)
            model: root.optionCount()
            currentIndex: root.highlightedIndex
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            delegate: Item {
                id: optionDelegate

                required property int index
                readonly property bool selected: index === root.currentIndex
                readonly property bool highlighted: index === root.highlightedIndex

                width: optionsView.width
                height: 36

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: optionDelegate.highlighted
                        ? Theme.accentSoft
                        : (optionDelegate.selected ? Theme.popupItemFill : "transparent")
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.optionText(optionDelegate.index)
                    color: Theme.textPrimary
                    font.pixelSize: root.font.pixelSize
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: root.highlightedIndex = optionDelegate.index
                    onClicked: root.activateOption(optionDelegate.index)
                }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
