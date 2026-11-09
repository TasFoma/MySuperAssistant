import QtQuick
import QtQuick.Layouts

SurfaceCard {
    id: root
    property string title: ""
    property string details: ""
    property bool stale: false
    ColumnLayout {
        width: parent.width
        spacing: Theme.spaceSmall
        SectionHeader { Layout.fillWidth: true; title: root.title; description: root.details }
        Text {
            Layout.fillWidth: true
            visible: root.stale
            text: "Данные не обновлены"
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
