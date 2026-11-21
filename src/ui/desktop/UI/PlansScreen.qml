import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Desktop 1.0

Item {
    id: root
    required property var client
    required property var desktop
    objectName: "plansScreen"
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceLarge
        SectionHeader {
            prominent: true
            Layout.fillWidth: true
            title: "Планы"
            description: "Меньше суеты, больше ясности"
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            Repeater {
                model: root.desktop.planSections
                AppButton {
                    required property var modelData
                    text: modelData.title
                    objectName: "planTab_" + modelData.key
                    tone: root.desktop.planSection === modelData.section ? "primary" : "secondary"
                    onClicked: root.desktop.selectPlanSection(modelData.section)
                }
            }
            Item {
                Layout.fillWidth: true
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Controls.ScrollView {
                id: tasksScroll
                objectName: "tasksSection"
                anchors.fill: parent
                visible: root.desktop.planSection === DesktopState.Tasks
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: tasksScroll.availableWidth
                    spacing: Theme.spaceLarge
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "Задачи"
                    }
                    AppButton {
                        text: "Добавить задачу"
                        objectName: "newTaskButton"
                        enabled: root.client.serverActionsEnabled
                        onClicked: root.desktop.planner.newTask()
                    }
                    EmptyState {
                        Layout.fillWidth: true
                        visible: root.client.tasks.count === 0
                        title: "Нет загруженных задач"
                    }
                    Repeater {
                        model: root.client.tasks
                        ColumnLayout {
                            id: task
                            required property var model
                            Layout.fillWidth: true
                            spacing: Theme.spaceTiny
                            TaskRow {
                                Layout.fillWidth: true
                                taskIdentifier: task.model.identifier
                                title: task.model.title
                                done: task.model.done
                                dueText: task.model.detailsText
                                actionsEnabled: root.client.serverActionsEnabled
                                stale: root.client.tasks.stale
                                onDoneRequested: function (identifier, done) {
                                    root.client.setTaskDone(identifier, done);
                                }
                            }
                            RecordActions {
                                actionsEnabled: root.client.serverActionsEnabled
                                onEditRequested: root.desktop.planner.editTask(task.model.identifier)
                                onDeleteRequested: root.desktop.planner.deleteTask(task.model.identifier)
                            }
                        }
                    }
                }
            }
            Controls.ScrollView {
                id: calendarScroll
                objectName: "calendarSection"
                anchors.fill: parent
                visible: root.desktop.planSection === DesktopState.Calendar
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: calendarScroll.availableWidth
                    spacing: Theme.spaceLarge
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "Календарь"
                        description: "События сегодня и выбранного дня"
                    }
                    AppButton {
                        text: "Добавить событие"
                        objectName: "newEventButton"
                        enabled: root.client.serverActionsEnabled
                        onClicked: root.desktop.planner.newEvent()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppTextField {
                            id: date
                            objectName: "calendarDate"
                            Layout.fillWidth: true
                            placeholderText: "ГГГГ-ММ-ДД"
                        }
                        AppButton {
                            text: "Показать день"
                            enabled: root.client.serverActionsEnabled
                            onClicked: root.desktop.showCalendarDay(date.text)
                        }
                    }
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "Сегодня"
                    }
                    Repeater {
                        model: root.client.eventsToday
                        EventRecord {
                            required property var model
                            Layout.fillWidth: true
                            eventData: model
                            editor: root.desktop.planner
                            actionsEnabled: root.client.serverActionsEnabled
                            stale: root.client.eventsToday.stale
                        }
                    }
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "Выбранный день"
                    }
                    EmptyState {
                        Layout.fillWidth: true
                        visible: root.client.calendarEvents.count === 0
                        title: "Нет загруженных событий"
                    }
                    Repeater {
                        model: root.client.calendarEvents
                        EventRecord {
                            required property var model
                            Layout.fillWidth: true
                            eventData: model
                            editor: root.desktop.planner
                            actionsEnabled: root.client.serverActionsEnabled
                            stale: root.client.calendarEvents.stale
                        }
                    }
                }
            }
            Controls.ScrollView {
                id: habitsScroll
                objectName: "habitsSection"
                anchors.fill: parent
                visible: root.desktop.planSection === DesktopState.Habits
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: habitsScroll.availableWidth
                    spacing: Theme.spaceLarge
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "Привычки"
                    }
                    AppButton {
                        text: "Добавить привычку"
                        objectName: "newHabitButton"
                        enabled: root.client.serverActionsEnabled
                        onClicked: root.desktop.planner.newHabit()
                    }
                    EmptyState {
                        Layout.fillWidth: true
                        visible: root.client.habits.count === 0
                        title: "Нет загруженных привычек"
                    }
                    Repeater {
                        model: root.client.habits
                        ColumnLayout {
                            id: habit
                            required property var model
                            Layout.fillWidth: true
                            spacing: Theme.spaceTiny
                            HabitProgress {
                                Layout.fillWidth: true
                                name: habit.model.name
                                progress: habit.model.progress
                                target: habit.model.target
                                amountText: habit.model.detailsText
                                stale: root.client.habits.stale
                            }
                            RecordActions {
                                primaryText: "Отметить"
                                actionsEnabled: root.client.serverActionsEnabled
                                onEditRequested: root.desktop.planner.recordHabit(habit.model.identifier)
                                onDeleteRequested: root.desktop.planner.deleteHabit(habit.model.identifier)
                            }
                        }
                    }
                }
            }
            RemindersScreen {
                objectName: "remindersSection"
                anchors.fill: parent
                visible: root.desktop.planSection === DesktopState.Reminders
                client: root.client
                editor: root.desktop.planner
            }
        }
    }
}
