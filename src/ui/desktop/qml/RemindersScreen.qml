import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    required property var client
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceLarge
        SectionHeader { Layout.fillWidth: true; title: "Напоминания"; description: "По времени, повторяющиеся и по месту" }
        EmptyState { Layout.fillWidth: true; visible: root.client.reminders.count === 0; title: "Нет загруженных напоминаний" }
        Repeater {
            model: root.client.reminders
            DetailsCard {
                required property var model
                Layout.fillWidth: true
                title: model.title
                details: model.detailsText
                stale: root.client.reminders.stale
            }
        }
    }
}
