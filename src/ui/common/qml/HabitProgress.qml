import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string name: ""
    property string amountText: ""
    property real progress: 0
    property real target: 1
    property bool stale: false
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceMedium
        Text {
            Layout.fillWidth: true
            text: root.name
            textFormat: Text.PlainText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Theme.fontWeightStrong
            wrapMode: Text.Wrap
        }
        Controls.ProgressBar {
            id: bar
            objectName: "habitBar"
            Layout.fillWidth: true
            from: 0
            to: root.target
            value: root.progress
            implicitHeight: Theme.progressHeight
            Accessible.name: root.name
            contentItem: Item {
                implicitHeight: Theme.progressHeight
                Rectangle {
                    width: bar.visualPosition * parent.width
                    height: parent.height
                    radius: Theme.progressHeight / 2
                    color: root.stale ? Theme.secondaryText : Theme.accent
                }
            }
            background: Rectangle {
                color: Theme.raised
                radius: Theme.progressHeight / 2
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.amountText
            textFormat: Text.PlainText
            color: Theme.secondaryText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
    }
}

