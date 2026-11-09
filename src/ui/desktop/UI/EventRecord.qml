import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0

ColumnLayout {
    id: root
    required property var eventData
    required property var editor
    property bool actionsEnabled: false
    property bool stale: false
    spacing: Theme.spaceTiny
    DetailsCard {
        Layout.fillWidth: true
        title: root.eventData.title
        details: root.eventData.detailsText
        stale: root.stale
    }
    RecordActions {
        actionsEnabled: root.actionsEnabled
        onEditRequested: root.editor.editEvent(root.eventData.identifier)
        onDeleteRequested: root.editor.deleteEvent(root.eventData.identifier)
    }
}
