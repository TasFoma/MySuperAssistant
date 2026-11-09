import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    required property var client
    required property var desktop
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceLarge
        SectionHeader { Layout.fillWidth: true; title: "Планы"; description: "Задачи, привычки и календарь" }
        SectionHeader { Layout.fillWidth: true; title: "Задачи" }
        EmptyState { Layout.fillWidth: true; visible: root.client.tasks.count === 0; title: "Нет загруженных задач" }
        Repeater {
            model: root.client.tasks
            TaskRow {
                required property string identifier
                required property string detailsText
                required property var model
                Layout.fillWidth: true
                taskIdentifier: identifier
                title: model.title
                done: model.done
                dueText: detailsText
                actionsEnabled: root.client.serverActionsEnabled
                stale: root.client.tasks.stale
                onDoneRequested: function(identifier, done) { root.client.setTaskDone(identifier, done) }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Привычки" }
        EmptyState { Layout.fillWidth: true; visible: root.client.habits.count === 0; title: "Нет загруженных привычек" }
        Repeater {
            model: root.client.habits
            HabitProgress {
                required property var model
                Layout.fillWidth: true
                name: model.name
                progress: model.progress
                target: model.target
                amountText: model.detailsText
                stale: root.client.habits.stale
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Календарь"; description: "События сегодня и выбранного дня" }
        RowLayout {
            Layout.fillWidth: true
            AppTextField { id: date; objectName: "calendarDate"; Layout.fillWidth: true; placeholderText: "ГГГГ-ММ-ДД" }
            AppButton {
                text: "Показать день"
                enabled: root.client.serverActionsEnabled
                onClicked: root.desktop.showCalendarDay(date.text)
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Сегодня" }
        Repeater {
            model: root.client.eventsToday
            DetailsCard {
                required property var model
                Layout.fillWidth: true
                title: model.title
                details: model.detailsText
                stale: root.client.eventsToday.stale
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Выбранный день" }
        EmptyState { Layout.fillWidth: true; visible: root.client.calendarEvents.count === 0; title: "Нет загруженных событий" }
        Repeater {
            model: root.client.calendarEvents
            DetailsCard {
                required property var model
                Layout.fillWidth: true
                title: model.title
                details: model.detailsText
                stale: root.client.calendarEvents.stale
            }
        }
    }
}
