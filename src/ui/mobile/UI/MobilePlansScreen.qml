import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Flickable {
    id: root
    objectName: "mobilePlansScroll"
    required property var client
    required property var mobile
    clip: true
    contentWidth: width
    contentHeight: contents.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    Controls.ScrollBar.vertical: Controls.ScrollBar { }
    ColumnLayout {
        id: contents
        width: root.width
        spacing: Theme.spaceMedium
        RowLayout {
            Layout.fillWidth: true
            SectionHeader { Layout.fillWidth: true; title: "Планы" }
            AppButton { text: "Обновить"; tone: "secondary"; enabled: root.client.serverActionsEnabled; onClicked: root.client.refresh() }
        }
        RowLayout {
            Layout.fillWidth: true
            AppButton { objectName: "mobilePreviousMonth"; text: "‹"; tone: "secondary"; Accessible.name: "Предыдущий месяц"; onClicked: root.mobile.shiftMonth(-1) }
            SectionHeader { Layout.fillWidth: true; description: root.mobile.monthTitle }
            AppButton { objectName: "mobileNextMonth"; text: "›"; tone: "secondary"; Accessible.name: "Следующий месяц"; onClicked: root.mobile.shiftMonth(1) }
        }
        GridLayout {
            id: calendar
            objectName: "mobileCalendar"
            Layout.fillWidth: true
            columns: Theme.calendarColumns
            columnSpacing: 0
            rowSpacing: Theme.spaceTiny
            Repeater {
                model: ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
                Text {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.secondaryText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            Repeater {
                model: root.mobile.calendarCells
                AppButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    padding: Theme.spaceTiny
                    text: modelData.label + (modelData.hasEvents ? " ·" : "")
                    enabled: modelData.valid
                    opacity: modelData.valid ? 1 : 0
                    tone: modelData.selected ? "primary" : "secondary"
                    Accessible.name: modelData.date + (modelData.today ? ", сегодня" : "") + (modelData.hasEvents ? ", есть события" : "")
                    onClicked: root.mobile.selectDay(modelData.date)
                }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "События"; description: root.client.calendarEvents.stale ? "Последние загруженные события · " + root.mobile.selectedDate : root.mobile.selectedDate }
        AppButton { objectName: "mobileNewEvent"; text: "Добавить событие"; enabled: root.client.serverActionsEnabled; onClicked: root.mobile.planner.newEvent() }
        EmptyState { Layout.fillWidth: true; visible: root.client.calendarEvents.count === 0; title: "Нет загруженных событий" }
        Repeater {
            model: root.client.calendarEvents
            ColumnLayout {
                id: eventRow
                required property var model
                Layout.fillWidth: true
                DetailsCard { Layout.fillWidth: true; title: eventRow.model.title; details: eventRow.model.detailsText; stale: root.client.calendarEvents.stale }
                RecordActions { actionsEnabled: root.client.serverActionsEnabled; onEditRequested: root.mobile.planner.editEvent(eventRow.model.identifier); onDeleteRequested: root.mobile.planner.deleteEvent(eventRow.model.identifier) }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Задачи" }
        AppButton { objectName: "mobileNewTask"; text: "Добавить задачу"; enabled: root.client.serverActionsEnabled; onClicked: root.mobile.planner.newTask() }
        EmptyState { Layout.fillWidth: true; visible: root.client.tasks.count === 0; title: "Нет загруженных задач" }
        Repeater {
            model: root.client.tasks
            ColumnLayout {
                id: taskRow
                required property var model
                Layout.fillWidth: true
                TaskRow {
                    Layout.fillWidth: true
                    taskIdentifier: taskRow.model.identifier
                    title: taskRow.model.title
                    done: taskRow.model.done
                    dueText: taskRow.model.detailsText
                    stale: root.client.tasks.stale
                    actionsEnabled: root.client.serverActionsEnabled
                    onDoneRequested: function(identifier, done) { root.client.setTaskDone(identifier, done) }
                }
                RecordActions { actionsEnabled: root.client.serverActionsEnabled; onEditRequested: root.mobile.planner.editTask(taskRow.model.identifier); onDeleteRequested: root.mobile.planner.deleteTask(taskRow.model.identifier) }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Привычки" }
        AppButton { objectName: "mobileNewHabit"; text: "Добавить привычку"; enabled: root.client.serverActionsEnabled; onClicked: root.mobile.planner.newHabit() }
        EmptyState { Layout.fillWidth: true; visible: root.client.habits.count === 0; title: "Нет загруженных привычек" }
        Repeater {
            model: root.client.habits
            ColumnLayout {
                id: habitRow
                required property var model
                Layout.fillWidth: true
                HabitProgress { Layout.fillWidth: true; name: habitRow.model.name; progress: habitRow.model.progress; target: habitRow.model.target; amountText: habitRow.model.detailsText; stale: root.client.habits.stale }
                RecordActions { primaryText: "Отметить"; actionsEnabled: root.client.serverActionsEnabled; onEditRequested: root.mobile.planner.recordHabit(habitRow.model.identifier); onDeleteRequested: root.mobile.planner.deleteHabit(habitRow.model.identifier) }
            }
        }
    }
}
