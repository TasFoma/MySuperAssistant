import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Rectangle {
    id: gallery
    width: 960
    height: implicitHeight
    implicitHeight: contents.implicitHeight + Theme.spacePage * 2
    color: Theme.background
    property int primaryClicks: 0
    property int disabledClicks: 0
    property int doneRequests: 0
    property string requestedIdentifier: ""
    property bool requestedDone: false
    property bool taskDone: false
    property bool actionsEnabled: true
    ColumnLayout {
        id: contents
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spacePage
        spacing: Theme.spaceSection
        SectionHeader {
            Layout.fillWidth: true
            title: "Общие компоненты"
            description: "MySuperAssistant · тёмная тема · тестовая галерея"
        }
        GridLayout {
            Layout.fillWidth: true
            columns: width >= Theme.cardWidth * 2 + Theme.spaceSection ? 2 : 1
            columnSpacing: Theme.spaceSection
            rowSpacing: Theme.spaceSection
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                spacing: Theme.spaceLarge
                SectionHeader { Layout.fillWidth: true; title: "Управление" }
                SurfaceCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spaceMedium
                        AppTextField {
                            objectName: "inputField"
                            Layout.fillWidth: true
                            placeholderText: "Название задачи"
                            text: "Прочитать главу"
                        }
                        AppTextField {
                            Layout.fillWidth: true
                            placeholderText: "Поле недоступно"
                            enabled: false
                        }
                        AppTextArea {
                            Layout.fillWidth: true
                            placeholderText: "Заметка"
                            text: "Небольшой шаг каждый день.\nПодробности можно записать здесь."
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spaceSmall
                            AppButton {
                                objectName: "primaryButton"
                                text: "Сохранить"
                                onClicked: gallery.primaryClicks++
                            }
                            AppButton { text: "Отмена"; tone: "secondary" }
                            AppButton {
                                objectName: "disabledButton"
                                text: "Недоступно"
                                enabled: false
                                onClicked: gallery.disabledClicks++
                            }
                        }
                    }
                }
                TaskRow {
                    objectName: "taskRow"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    taskIdentifier: "17"
                    title: "Выделить время на чтение и записать мысли"
                    dueText: "Сегодня, 19:00"
                    priorityText: "Высокий приоритет"
                    done: gallery.taskDone
                    actionsEnabled: gallery.actionsEnabled
                    stale: !clientState.online
                    onDoneRequested: function(identifier, done) {
                        gallery.doneRequests++
                        gallery.requestedIdentifier = identifier
                        gallery.requestedDone = done
                    }
                }
                HabitProgress {
                    objectName: "habitProgress"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    name: "Чтение"
                    progress: 3.5
                    target: 1.5
                    amountText: "3,5 / 1,5 главы"
                    stale: !clientState.online
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                spacing: Theme.spaceLarge
                SectionHeader { Layout.fillWidth: true; title: "Разговор и состояния" }
                MessageBubble {
                    objectName: "messageBubble"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    author: "Вы"
                    messageText: "<b>Обычный текст</b>\nЧто получилось сегодня? 🙂"
                    timeText: "19:10"
                    outgoing: true
                }
                MessageBubble {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    author: "Ассистент"
                    messageText: "Расскажи об одном небольшом достижении за день."
                    timeText: "19:11"
                }
                ConnectionBanner {
                    objectName: "connectionBanner"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    statusText: clientState.statusText
                    online: clientState.online
                    reconnectAvailable: clientState.reconnectAvailable
                    staleData: !clientState.online
                    onReconnectRequested: clientState.reconnect()
                }
                ErrorBanner {
                    objectName: "errorBanner"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    message: "Связь прервалась. Результат действия неизвестен. Обновите данные после подключения."
                }
                EmptyState {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    title: "Здесь пока пусто"
                    description: "Новые записи появятся после загрузки данных."
                }
            }
        }
    }
}
