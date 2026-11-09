import QtQuick
import QtQuick.Layouts

ColumnLayout {
    property string title: ""
    property string description: ""
    spacing: Theme.spaceTiny
    Text {
        Layout.fillWidth: true
        text: parent.title
        textFormat: Text.PlainText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontTitle
        font.weight: Theme.fontWeightStrong
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        visible: text.length > 0
        text: parent.description
        textFormat: Text.PlainText
        color: Theme.secondaryText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }
}

