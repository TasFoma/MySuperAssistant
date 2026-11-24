import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.ScrollView {
    id: root
    required property var sport
    property bool progress: false
    contentWidth: availableWidth
    clip: true
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceMedium
        SectionHeader {
            Layout.fillWidth: true
            title: root.progress ? "Динамика по упражнениям" : "Сохранённые занятия"
            description: root.sport.journal.errorText
        }
        SportFilterBox {
            Layout.fillWidth: true
            visible: root.progress
            model: root.sport.journal.progressExercises
            textRole: "label"
            valueRole: "value"
            currentIndex: {
                for (var index = 0; index < root.sport.journal.progressExercises.length; ++index)
                    if (root.sport.journal.progressExercises[index].value === root.sport.journal.progressExercise) return index;
                return -1;
            }
            Accessible.name: "Упражнение для просмотра динамики"
            onActivated: root.sport.journal.progressExercise = currentValue
        }
        EmptyState {
            Layout.fillWidth: true
            visible: root.progress ? root.sport.journal.progress.length === 0 : root.sport.journal.history.length === 0
            title: root.progress ? "Пока нет выполненных подходов" : "Пока нет сохранённых занятий"
            description: root.progress ? "Пропуски не учитываются как выполнение. Разные варианты и условия показываются отдельно." : "Завершённое занятие доступно здесь без подключения к серверу."
        }
        Repeater {
            model: root.progress ? [] : root.sport.journal.history
            SurfaceCard {
                id: workoutCard
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSmall
                    SectionHeader { Layout.fillWidth: true; title: workoutCard.modelData.title; description: workoutCard.modelData.status }
                    AppButton { text: "Состав и результаты"; tone: "secondary"; onClicked: root.sport.journal.openWorkout(workoutCard.modelData.identifier) }
                }
            }
        }
        Repeater {
            model: root.progress ? root.sport.journal.progress : []
            SurfaceCard {
                id: seriesCard
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSmall
                    SectionHeader { Layout.fillWidth: true; title: seriesCard.modelData.title; description: seriesCard.modelData.conditions }
                    Repeater {
                        model: seriesCard.modelData.points
                        AppButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.text
                            tone: "secondary"
                            onClicked: root.sport.journal.openWorkout(modelData.workoutId)
                        }
                    }
                }
            }
        }
    }
}
