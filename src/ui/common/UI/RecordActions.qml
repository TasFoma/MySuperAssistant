import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property bool actionsEnabled: false
    property string primaryText: "Изменить"
    signal editRequested()
    signal deleteRequested()
    spacing: Theme.spaceSmall
    AppButton {
        text: root.primaryText
        visible: text.length > 0
        tone: "secondary"
        enabled: root.actionsEnabled
        onClicked: root.editRequested()
    }
    AppButton {
        text: "Удалить"
        tone: "danger"
        enabled: root.actionsEnabled
        onClicked: root.deleteRequested()
    }
}
