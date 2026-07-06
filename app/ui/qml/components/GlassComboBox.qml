import QtQuick
import QtQuick.Controls
import Opusora

ComboBox {
    id: root

    implicitHeight: 36
    implicitWidth: 180
    leftPadding: 14
    rightPadding: 34
    font.pixelSize: 13

    delegate: ItemDelegate {
        id: itemDelegate

        required property int index
        property string optionText: {
            if (typeof modelData === "object" && root.textRole.length > 0) {
                return modelData[root.textRole]
            }
            return modelData
        }

        width: root.width
        height: 36
        leftPadding: 12
        rightPadding: 12
        text: optionText
        highlighted: root.highlightedIndex === index

        background: Rectangle {
            radius: 8
            color: itemDelegate.highlighted
                ? Theme.accentSoft
                : (itemDelegate.hovered
                    ? Theme.popupItemHover
                    : (itemDelegate.index === root.currentIndex ? Theme.popupItemFill : "transparent"))
        }

        contentItem: Text {
            text: itemDelegate.text
            color: Theme.textPrimary
            font.pixelSize: root.font.pixelSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    indicator: Text {
        x: root.width - width - 13
        y: Math.round((root.height - height) / 2)
        text: "⌄"
        color: Theme.textSecondary
        font.pixelSize: 15
        font.weight: Font.DemiBold
    }

    contentItem: Item {
        implicitWidth: displayLabel.implicitWidth + 48
        implicitHeight: 36

        Text {
            id: displayLabel
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 34
            anchors.verticalCenter: parent.verticalCenter
            text: root.displayText
            color: root.enabled ? Theme.textPrimary : Theme.textSecondary
            opacity: root.enabled ? 1.0 : 0.62
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: height / 2
        color: root.down || root.hovered ? Theme.controlHover : Theme.controlFill
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

    popup: Popup {
        y: root.height + 6
        z: 1000
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 268)
        padding: 6

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
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
