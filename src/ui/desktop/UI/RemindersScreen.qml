import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    required property var client
    required property var editor
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceLarge
        SectionHeader { Layout.fillWidth: true; title: "Напоминания"; description: "По времени, повторяющиеся и по месту" }
        AppButton { text: "Добавить напоминание"; objectName: "newReminderButton"; enabled: root.client.serverActionsEnabled; onClicked: root.editor.newReminder() }
        EmptyState { Layout.fillWidth: true; visible: root.client.reminders.count === 0; title: "Нет загруженных напоминаний" }
        Repeater {
            model: root.client.reminders
            ColumnLayout {
                id: reminder
                required property var model
                Layout.fillWidth: true
                spacing: Theme.spaceTiny
                DetailsCard {
                    Layout.fillWidth: true
                    title: reminder.model.title
                    details: reminder.model.detailsText
                    stale: root.client.reminders.stale
                }
                RecordActions {
                    primaryText: ""
                    actionsEnabled: root.client.serverActionsEnabled
                    onDeleteRequested: root.editor.deleteReminder(reminder.model.identifier)
                }
            }
        }
    }
}
