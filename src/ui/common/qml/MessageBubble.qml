import QtQuick
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string author: ""
    property string messageText: ""
    property string timeText: ""
    property bool outgoing: false
    surfaceColor: outgoing ? Theme.userBubble : Theme.surface
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceSmall
        Text {
            Layout.fillWidth: true
            text: root.author
            textFormat: Text.PlainText
            color: Theme.secondaryText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.weight: Theme.fontWeightStrong
        }
        AppTextArea {
            objectName: "messageBody"
            Layout.fillWidth: true
            text: root.messageText
            readOnly: true
            padding: 0
            implicitHeight: contentHeight
            background: null
            Accessible.name: "Сообщение: " + root.author
        }
        Text {
            Layout.fillWidth: true
            text: root.timeText
            visible: text.length > 0
            textFormat: Text.PlainText
            horizontalAlignment: Text.AlignRight
            color: Theme.secondaryText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}

