import QtQuick
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string message: ""
    property string actionText: ""
    property bool actionEnabled: false
    signal actionRequested()
    visible: message.length > 0
    surfaceColor: Theme.dangerSurface
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceMedium
        Text {
            Layout.fillWidth: true
            text: root.message
            textFormat: Text.PlainText
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.Wrap
        }
        AppButton {
            objectName: "errorActionButton"
            visible: root.actionText.length > 0
            enabled: root.actionEnabled
            text: root.actionText
            tone: "secondary"
            onClicked: root.actionRequested()
        }
    }
}

