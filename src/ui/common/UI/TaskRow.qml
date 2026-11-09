import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string taskIdentifier: ""
    property string title: ""
    property string dueText: ""
    property string priorityText: ""
    property bool done: false
    property bool actionsEnabled: false
    property bool stale: false
    signal doneRequested(string identifier, bool done)
    RowLayout {
        width: parent.width
        spacing: Theme.spaceMedium
        Controls.CheckBox {
            id: check
            objectName: "taskCheckBox"
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Theme.controlHeight
            Layout.preferredHeight: Theme.controlHeight
            checked: root.done
            enabled: root.actionsEnabled
            nextCheckState: function() { return root.done ? Qt.Checked : Qt.Unchecked }
            onClicked: root.doneRequested(root.taskIdentifier, !root.done)
            Accessible.name: root.title
            indicator: Rectangle {
                implicitWidth: Theme.indicatorSize
                implicitHeight: Theme.indicatorSize
                x: (check.width - width) / 2
                y: (check.height - height) / 2
                radius: Theme.radiusControl
                color: check.checked ? Theme.accent : Theme.raised
                border.width: check.visualFocus ? Theme.focusWidth : Theme.borderWidth
                border.color: check.visualFocus ? Theme.accent : check.enabled ? Theme.secondaryText : Theme.border
                Text {
                    anchors.centerIn: parent
                    text: check.checked ? "✓" : ""
                    textFormat: Text.PlainText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    color: Theme.accentText
                }
            }
            contentItem: Item { }
            background: Item { }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceTiny
            Text {
                Layout.fillWidth: true
                text: root.title
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: root.done ? Theme.secondaryText : Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.strikeout: root.done
            }
            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.dueText
                textFormat: Text.PlainText
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.priorityText
                textFormat: Text.PlainText
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.Wrap
            }
            Text {
                visible: root.stale
                text: "Данные не обновлены"
                textFormat: Text.PlainText
                color: Theme.warning
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}

