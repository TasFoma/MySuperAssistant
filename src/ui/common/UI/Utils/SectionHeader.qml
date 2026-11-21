import QtQuick
import QtQuick.Layouts

ColumnLayout {
    property string title: ""
    property string description: ""
    property bool prominent: false
    spacing: Theme.spaceSmall
    Text {
        Layout.fillWidth: true
        text: parent.title
        visible: text.length > 0
        textFormat: Text.PlainText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: parent.prominent ? Theme.fontHeading : Theme.fontTitle
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

