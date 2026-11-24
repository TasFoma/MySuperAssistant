import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.ScrollView {
    id: root
    required property var sport
    readonly property var editor: sport.journal.programEditor
    contentWidth: availableWidth
    clip: true
    ColumnLayout {
        width: Math.min(root.availableWidth, Theme.formWidth)
        x: (root.availableWidth - width) / 2
        spacing: Theme.spaceMedium
        SectionHeader { Layout.fillWidth: true; title: "Своя программа"; description: root.editor.status }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            AppButton { text: "К программам"; objectName: "sportCloseProgram"; onClicked: root.editor.close() }
            AppButton { text: "Сохранить программу"; tone: "primary"; objectName: "sportSaveProgram"; onClicked: root.editor.save() }
            AppButton { text: "Сохранить черновик"; onClicked: root.editor.retry() }
        }
        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight + Theme.spaceSmall
            contentWidth: sessionRow.implicitWidth
            clip: true
            RowLayout {
                id: sessionRow
                Repeater {
                    model: root.editor.sessions
                    AppButton {
                        required property var modelData
                        text: modelData.title || "Занятие без названия"
                        tone: root.editor.sessionIndex === modelData.index ? "primary" : "secondary"
                        onClicked: root.editor.selectSession(modelData.index)
                    }
                }
            }
        }
        Repeater {
            model: root.editor.fields
            SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            AppButton { text: "Добавить занятие"; onClicked: root.editor.requestSession() }
            AppButton { text: "Занятие выше"; tone: "secondary"; onClicked: root.editor.moveSession(-1) }
            AppButton { text: "Занятие ниже"; tone: "secondary"; onClicked: root.editor.moveSession(1) }
            AppButton { text: "Убрать занятие"; onClicked: root.editor.removeSession() }
        }
        Repeater {
            model: root.editor.items
            SurfaceCard {
                id: itemCard
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceMedium
                    SectionHeader { Layout.fillWidth: true; title: itemCard.modelData.title }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        AppButton { text: "Техника"; onClicked: root.editor.showTechnique(itemCard.modelData.identifier) }
                        AppButton { text: "Выше"; onClicked: root.editor.moveItem(itemCard.modelData.identifier, -1) }
                        AppButton { text: "Ниже"; onClicked: root.editor.moveItem(itemCard.modelData.identifier, 1) }
                        AppButton { text: "Убрать упражнение"; onClicked: root.editor.removeItem(itemCard.modelData.identifier) }
                    }
                    Repeater {
                        model: itemCard.modelData.conditions
                        SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root }
                    }
                    Repeater {
                        model: itemCard.modelData.sets
                        ColumnLayout {
                            id: setRow
                            required property var modelData
                            Layout.fillWidth: true
                            SectionHeader { Layout.fillWidth: true; title: "Плановый подход"; description: setRow.modelData.side }
                            Repeater {
                                model: setRow.modelData.fields
                                SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root }
                            }
                            AppButton { text: "Убрать подход"; onClicked: root.editor.removeSet(itemCard.modelData.identifier, setRow.modelData.identifier) }
                        }
                    }
                    AppButton { text: "Добавить подход"; onClicked: root.editor.requestSet(itemCard.modelData.identifier) }
                }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Добавить упражнение"; description: "Повторения и вес задаёте вы. Пустые поля сохраняются в черновике." }
        AppTextField {
            Layout.fillWidth: true
            placeholderText: "Найти упражнение"
            text: root.sport.query
            onTextEdited: root.sport.query = text
        }
        AppButton { text: "Все доступные упражнения"; tone: "secondary"; onClicked: root.sport.resetFilters() }
        Repeater {
            model: root.sport.exercises
            AppButton {
                required property var modelData
                Layout.fillWidth: true
                text: modelData.title
                objectName: "sportAddExercise_" + modelData.identifier
                onClicked: root.editor.requestExercise(modelData.identifier)
            }
        }
    }
}
