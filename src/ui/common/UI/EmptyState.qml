import QtQuick
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string title: ""
    property string description: ""
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceSmall
        Text {
            Layout.fillWidth: true
            text: root.title
            textFormat: Text.PlainText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Theme.fontWeightStrong
            wrapMode: Text.Wrap
        }
        Text {
            Layout.fillWidth: true
            text: root.description
            visible: text.length > 0
            textFormat: Text.PlainText
            color: Theme.secondaryText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.Wrap
        }
    }
}

