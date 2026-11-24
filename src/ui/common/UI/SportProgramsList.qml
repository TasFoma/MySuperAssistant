import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.ScrollView {
    id: root
    required property var sport
    contentWidth: availableWidth
    clip: true
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceMedium
        SectionHeader { Layout.fillWidth: true; title: "Мои программы"; description: root.sport.journal.errorText }
        AppButton { text: "Создать программу"; tone: "primary"; enabled: root.sport.storageReady; objectName: "sportCreateProgram"; onClicked: root.sport.journal.createProgram() }
        Repeater {
            model: root.sport.journal.drafts
            AppButton {
                required property var modelData
                Layout.fillWidth: true
                text: "Продолжить черновик: " + modelData.title
                onClicked: root.sport.journal.resumeProgram(modelData.identifier)
            }
        }
        Repeater {
            model: root.sport.journal.programs
            SurfaceCard {
                id: programCard
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceMedium
                    SectionHeader { Layout.fillWidth: true; title: programCard.modelData.title }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        AppButton { text: "Изменить"; onClicked: root.sport.journal.editProgram(programCard.modelData.identifier) }
                        AppButton { text: "Создать копию"; onClicked: root.sport.journal.copyProgram(programCard.modelData.identifier) }
                    }
                    Repeater {
                        model: programCard.modelData.sessions
                        AppButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: "Начать: " + modelData.title
                            onClicked: root.sport.journal.startWorkout(programCard.modelData.identifier, modelData.index)
                        }
                    }
                }
            }
        }
        EmptyState {
            Layout.fillWidth: true
            title: "Готовые P1 и P3 готовятся"
            description: "Состав и плановые подходы ещё уточняются. Можно составить собственную программу из доступных карточек."
        }
    }
}
