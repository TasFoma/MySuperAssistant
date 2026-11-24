import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

Controls.ScrollView {
    id: root
    required property var sport
    property var editor: sport.journal.workoutEditor
    property bool additionVisible: false
    contentWidth: availableWidth
    clip: true
    objectName: "sportWorkoutScroll"
    ColumnLayout {
        width: Math.min(root.availableWidth, Theme.formWidth)
        x: (root.availableWidth - width) / 2
        spacing: Theme.spaceMedium
        SectionHeader { Layout.fillWidth: true; title: root.editor.title; description: root.editor.status }
        SurfaceCard {
            objectName: "sportCorrectionDiscardPrompt"
            visible: root.editor.discardRequested
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceSmall
                SectionHeader { Layout.fillWidth: true; title: "Отменить несохранённые исправления?"; description: "Сохранённый результат останется прежним." }
                AppButton { text: "Продолжить исправление"; onClicked: root.editor.keepEditing() }
                AppButton { text: "Отменить исправления"; objectName: "sportDiscardCorrectionConfirm"; tone: "secondary"; onClicked: root.editor.discardCorrection() }
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            AppButton { text: "Назад"; tone: "secondary"; objectName: "sportCloseWorkout"; onClicked: root.editor.close() }
            AppButton { visible: root.editor.editing; text: "Повторить сохранение"; tone: "secondary"; onClicked: root.editor.retry() }
            AppButton { visible: root.editor.correcting; text: "Отменить исправления"; tone: "secondary"; onClicked: root.editor.requestDiscard() }
            AppButton { visible: !root.editor.editing; text: "Исправить результат"; onClicked: root.editor.beginCorrection() }
        }
        SurfaceCard {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceSmall
                SectionHeader {
                    Layout.fillWidth: true
                    title: "Общее время: " + root.editor.timeSummary.total
                    description: "Без пауз: " + root.editor.timeSummary.active + " · Ручная длительность: " + root.editor.timeSummary.corrected
                }
                SectionHeader {
                    visible: root.editor.timeSummary.warning
                    Layout.fillWidth: true
                    description: "Часы изменились или длительность недостоверна. Проверьте интервалы и при необходимости укажите итоговую длительность вручную."
                }
                AppButton {
                    visible: root.editor.active
                    text: root.editor.timeSummary.paused ? "Продолжить таймер" : "Пауза"
                    objectName: "sportPauseWorkout"
                    onClicked: root.editor.pauseOrResume()
                }
            }
        }
        SectionHeader { Layout.fillWidth: true; visible: root.editor.restrictionsSnapshot.length > 0; title: "Ограничения на момент начала"; description: root.editor.restrictionsSnapshot }
        Repeater {
            model: root.editor.absences
            SurfaceCard {
                id: gap
                required property var modelData
                property bool detailsVisible: !modelData.resolved
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSmall
                    SectionHeader { Layout.fillWidth: true; title: gap.modelData.title; description: gap.modelData.summary }
                    AppButton { visible: root.editor.active && gap.modelData.resolved; text: "Изменить учёт интервала"; tone: "secondary"; onClicked: gap.detailsVisible = !gap.detailsVisible }
                    SportFormField { visible: root.editor.active && gap.detailsVisible; field: gap.modelData.field; editor: root.editor; viewport: root }
                    Flow {
                        visible: root.editor.active && gap.detailsVisible
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        AppButton { text: "Считать активностью"; tone: "secondary"; onClicked: root.editor.resolveAbsence(gap.modelData.identifier, "activity") }
                        AppButton { text: "Считать паузой"; tone: "secondary"; onClicked: root.editor.resolveAbsence(gap.modelData.identifier, "pause") }
                        AppButton { text: "Указать длительность вручную"; tone: "secondary"; onClicked: root.editor.resolveAbsence(gap.modelData.identifier, "manual") }
                    }
                }
            }
        }
        Repeater {
            model: root.editor.items
            SurfaceCard {
                id: exerciseCard
                required property var modelData
                property bool replacementVisible: false
                property bool actionsVisible: false
                property bool conditionsVisible: false
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceMedium
                    SectionHeader {
                        Layout.fillWidth: true
                        title: exerciseCard.modelData.title + (exerciseCard.modelData.replacement ? " · замена" : "")
                        description: exerciseCard.modelData.brief
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        AppButton { text: "Техника"; tone: "secondary"; onClicked: root.editor.showTechnique(exerciseCard.modelData.identifier) }
                        AppButton { text: "Условия"; tone: "secondary"; onClicked: exerciseCard.conditionsVisible = !exerciseCard.conditionsVisible }
                        AppButton { visible: root.editor.editing; text: "Ещё"; tone: "secondary"; onClicked: exerciseCard.actionsVisible = !exerciseCard.actionsVisible }
                    }
                    Flow {
                        Layout.fillWidth: true
                        visible: exerciseCard.actionsVisible && root.editor.editing
                        spacing: Theme.spaceSmall
                        AppButton { visible: root.editor.editing; text: "Пропустить оставшееся"; tone: "secondary"; onClicked: root.editor.skipExercise(exerciseCard.modelData.identifier) }
                        AppButton { visible: root.editor.editing; text: "Выбрать замену"; tone: "secondary"; onClicked: exerciseCard.replacementVisible = !exerciseCard.replacementVisible }
                        AppButton { visible: root.editor.correcting; text: "Убрать результат упражнения"; tone: "secondary"; onClicked: root.editor.removeItem(exerciseCard.modelData.identifier) }
                    }
                    ColumnLayout {
                        visible: exerciseCard.replacementVisible && root.editor.editing
                        Layout.fillWidth: true
                        SectionHeader { Layout.fillWidth: true; description: "Задайте план замены и выберите упражнение. Вес не переносится, фактические значения задаёте вы." }
                        Repeater {
                            model: exerciseCard.modelData.replacementFields
                            SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root }
                        }
                        AppButton { text: "Все доступные упражнения"; tone: "secondary"; onClicked: root.sport.resetFilters() }
                        Repeater {
                            model: root.sport.exercises
                            AppButton {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.title
                                tone: "secondary"
                                onClicked: root.editor.requestReplacement(exerciseCard.modelData.identifier, modelData.identifier)
                            }
                        }
                    }
                    Repeater {
                        model: exerciseCard.modelData.sets
                        ColumnLayout {
                            id: setCard
                            required property var modelData
                            property bool extrasVisible: modelData.extraLoadPresent
                            Layout.fillWidth: true
                            spacing: Theme.spaceSmall
                            SectionHeader { Layout.fillWidth: true; title: setCard.modelData.plan; description: "Факт: " + setCard.modelData.fact }
                            Repeater {
                                model: root.editor.editing ? setCard.modelData.fields : []
                                SportFormField { required property var modelData; visible: !modelData.optionalLoad || setCard.extrasVisible; field: modelData; editor: root.editor; viewport: root }
                            }
                            AppButton { visible: root.editor.editing && setCard.modelData.bodyweight; text: "Отягощение / противовес"; tone: "secondary"; onClicked: setCard.extrasVisible = !setCard.extrasVisible }
                            Flow {
                                visible: root.editor.editing
                                Layout.fillWidth: true
                                spacing: Theme.spaceSmall
                                AppButton { text: "Выполнен"; onClicked: root.editor.recordSet(exerciseCard.modelData.identifier, setCard.modelData.identifier) }
                                AppButton { text: "Пропущен"; tone: "secondary"; onClicked: root.editor.skipSet(exerciseCard.modelData.identifier, setCard.modelData.identifier) }
                                AppButton { visible: root.editor.correcting; text: "Убрать подход"; tone: "secondary"; onClicked: root.editor.removeSet(exerciseCard.modelData.identifier, setCard.modelData.identifier) }
                            }
                        }
                    }
                    AppButton { visible: root.editor.editing; text: "Добавить подход"; tone: "secondary"; onClicked: root.editor.requestSet(exerciseCard.modelData.identifier) }
                    ColumnLayout {
                        visible: exerciseCard.conditionsVisible
                        Layout.fillWidth: true
                        Repeater {
                            model: exerciseCard.modelData.conditions
                            SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root; readOnly: !root.editor.editing }
                        }
                    }
                }
            }
        }
        AppButton { visible: root.editor.editing; text: "Добавить упражнение"; tone: "secondary"; onClicked: root.additionVisible = !root.additionVisible }
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.editor.editing && (root.additionVisible || root.editor.items.length === 0)
            SectionHeader { Layout.fillWidth: true; title: "Дополнительное упражнение"; description: "Задайте план и выберите упражнение. Вес не назначается автоматически." }
            Repeater {
                model: root.editor.additionFields
                SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root }
            }
            AppTextField { Layout.fillWidth: true; placeholderText: "Найти упражнение"; text: root.sport.query; onTextEdited: root.sport.query = text }
            AppButton { text: "Все доступные упражнения"; tone: "secondary"; onClicked: root.sport.resetFilters() }
            Repeater {
                model: parent.visible ? root.sport.exercises : []
                AppButton { required property var modelData; Layout.fillWidth: true; text: modelData.title; tone: "secondary"; onClicked: { root.editor.requestAdditionalExercise(modelData.identifier); root.additionVisible = false; } }
            }
        }
        SectionHeader { Layout.fillWidth: true; title: "Состояние и итог занятия"; description: "Оценки необязательны. Они не назначают и не увеличивают нагрузку." }
        Repeater {
            model: root.editor.wellbeing
            SurfaceCard {
                id: wellbeingCard
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSmall
                    SectionHeader { Layout.fillWidth: true; title: wellbeingCard.modelData.title }
                    Repeater {
                        model: wellbeingCard.modelData.fields
                        SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root; readOnly: !root.editor.editing }
                    }
                    SectionHeader { Layout.fillWidth: true; title: "Области боли (можно несколько)"; description: wellbeingCard.modelData.areaSummary }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        visible: root.editor.editing
                        Repeater {
                            model: wellbeingCard.modelData.areas
                            AppButton {
                                required property var modelData
                                text: modelData.title
                                tone: modelData.selected ? "primary" : "secondary"
                                enabled: root.editor.editing
                                onClicked: root.editor.toggleArea(wellbeingCard.modelData.prefix, modelData.key)
                            }
                        }
                    }
                    SectionHeader { Layout.fillWidth: true; title: "Новая боль"; description: wellbeingCard.modelData.newPainSummary }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSmall
                        visible: root.editor.editing
                        Repeater {
                            model: [{key:"",title:"Не указано"},{key:"yes",title:"Да"},{key:"no",title:"Нет"}]
                            AppButton {
                                required property var modelData
                                text: modelData.title
                                tone: wellbeingCard.modelData.newPain === modelData.key ? "primary" : "secondary"
                                enabled: root.editor.editing
                                onClicked: root.editor.setField(wellbeingCard.modelData.prefix + "/newPain", modelData.key)
                            }
                        }
                    }
                }
            }
        }
        Repeater {
            model: root.editor.fields
            SportFormField { required property var modelData; field: modelData; editor: root.editor; viewport: root; readOnly: !root.editor.editing }
        }
        SectionHeader { visible: root.editor.editing; Layout.fillWidth: true; description: "Перед сохранением проверьте дату, факт подходов и длительность. Неотмеченные подходы нужно выполнить или явно пропустить." }
        SectionHeader { visible: root.editor.editing; Layout.fillWidth: true; description: root.editor.status }
        AppButton {
            visible: root.editor.editing
            Layout.fillWidth: true
            objectName: "sportFinishWorkout"
            text: root.editor.correcting ? "Сохранить исправление" : "Завершить занятие"
            onClicked: root.editor.finish()
        }

    }
}
