import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Flickable {
    id: root
    objectName: "mobileRemindersScroll"
    required property var client
    required property var editor
    clip: true
    contentWidth: width
    contentHeight: contents.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    Controls.ScrollBar.vertical: Controls.ScrollBar { }
    ColumnLayout {
        id: contents
        width: root.width
        spacing: Theme.spaceMedium
        SectionHeader { Layout.fillWidth: true; title: "Напоминания"; description: "По времени, повторяющиеся и по месту" }
        AppButton { objectName: "mobileNewReminder"; text: "Добавить напоминание"; enabled: root.client.serverActionsEnabled; onClicked: root.editor.newReminder() }
        EmptyState { Layout.fillWidth: true; visible: root.client.reminders.count === 0; title: "Нет загруженных напоминаний" }
        Repeater {
            model: root.client.reminders
            ColumnLayout {
                id: reminderRow
                required property var model
                Layout.fillWidth: true
                DetailsCard { Layout.fillWidth: true; title: reminderRow.model.title; details: reminderRow.model.detailsText; stale: root.client.reminders.stale }
                RecordActions { primaryText: ""; actionsEnabled: root.client.serverActionsEnabled; onDeleteRequested: root.editor.deleteReminder(reminderRow.model.identifier) }
            }
        }
    }
}
