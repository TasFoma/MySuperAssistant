import QtQuick
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string statusText: ""
    property bool online: false
    property bool reconnectAvailable: false
    property bool staleData: false
    signal reconnectRequested()
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceMedium
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            Rectangle {
                Layout.preferredWidth: Theme.statusDotSize
                Layout.preferredHeight: Theme.statusDotSize
                radius: Theme.statusDotSize / 2
                color: root.online ? Theme.success : Theme.warning
            }
            Text {
                Layout.fillWidth: true
                text: root.statusText
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
        }
        Text {
            Layout.fillWidth: true
            visible: root.staleData
            text: "Показаны последние загруженные данные"
            textFormat: Text.PlainText
            color: Theme.secondaryText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
        AppButton {
            objectName: "reconnectButton"
            visible: root.reconnectAvailable
            text: "Переподключиться"
            tone: "secondary"
            onClicked: root.reconnectRequested()
        }
    }
}

