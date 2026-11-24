import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Item {
    id: root
    required property var sport
    readonly property bool wide: width >= Theme.cardWidth * 2 + Theme.spaceLarge
    property bool filtersExpanded: false
    readonly property var details: sport.selected
    readonly property string selectedIdentifier: details.identifier || ""
    onSelectedIdentifierChanged: Qt.callLater(function() {
        if (detailScroll.contentItem) detailScroll.contentItem.contentY = 0;
    })
    objectName: "sportLibraryPanel"
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMedium
        SectionHeader {
            Layout.fillWidth: true
            prominent: true
            title: "Тренировки"
            description: root.sport.summary
        }
        ErrorBanner {
            Layout.fillWidth: true
            message: root.sport.errorText
            actionText: "Повторить загрузку"
            actionEnabled: true
            onActionRequested: root.sport.retry()
        }
        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight + Theme.spaceSmall
            contentWidth: sectionsRow.implicitWidth
            contentHeight: Theme.controlHeight
            clip: true
            RowLayout {
                id: sectionsRow
                spacing: Theme.spaceSmall
                Repeater {
                    model: root.sport.sections
                    AppButton {
                        required property var modelData
                        text: modelData.title
                        tone: root.sport.section === modelData.key ? "primary" : "secondary"
                        objectName: "sportSection_" + modelData.key
                        onClicked: root.sport.navigate(modelData.key)
                    }
                }
            }
        }
        Controls.ScrollView {
            id: overview
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.sport.section === "overview"
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: overview.availableWidth
                spacing: Theme.spaceLarge
                SurfaceCard {
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spaceMedium
                        SectionHeader {
                            Layout.fillWidth: true
                            title: "Техника под рукой"
                            description: "Проверенные текстовые карточки доступны без связи с сервером. Избранное и предпочтения сохраняются на устройстве."
                        }
                        AppButton {
                            text: "Открыть упражнения"
                            objectName: "sportOpenExercises"
                            onClicked: root.sport.openExercises()
                        }
                    }
                }
                EmptyState {
                    Layout.fillWidth: true
                    title: "Следующий шаг — запись занятия"
                    description: "Экран ввода тренировки ещё готовится. Здесь нет вымышленных результатов или показателей."
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.sport.section === "programs"
            spacing: Theme.spaceMedium
            RowLayout {
                Layout.fillWidth: true
                visible: root.wide || !root.sport.hasSelection || !root.sport.showExercises
                AppButton {
                    text: "Упражнения"
                    tone: root.sport.showExercises ? "primary" : "secondary"
                    onClicked: root.sport.openExercises()
                }
                AppButton {
                    text: "Готовые"
                    Accessible.name: "Готовые программы"
                    tone: !root.sport.showExercises ? "primary" : "secondary"
                    onClicked: root.sport.openPrograms()
                }
            }
            EmptyState {
                Layout.fillWidth: true
                visible: !root.sport.showExercises
                title: "P1 и P3 готовятся"
                description: "Выбраны занятия с опорой и занятия с гантелями. Полные карточки, состав и плановые подходы ещё проверяются. Индивидуальные веса приложение не назначает."
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.sport.showExercises
                spacing: Theme.spaceSmall
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.wide || !root.sport.hasSelection
                    AppTextField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        placeholderText: "Поиск упражнений"
                        text: root.sport.query
                        objectName: "sportSearch"
                        Accessible.name: "Поиск упражнений"
                        onTextEdited: root.sport.query = text
                    }
                    AppButton {
                        text: "Фильтры"
                        tone: root.filtersExpanded ? "primary" : "secondary"
                        objectName: "sportToggleFilters"
                        onClicked: root.filtersExpanded = !root.filtersExpanded
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.filtersExpanded && !Qt.inputMethod.visible && (root.wide || !root.sport.hasSelection)
                    SportFilterBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: root.sport.equipmentOptions
                        currentIndex: root.sport.equipmentIndex
                        Accessible.name: "Доступный инвентарь"
                        objectName: "sportEquipmentFilter"
                        onActivated: root.sport.setEquipmentFilter(currentText)
                    }
                    SportFilterBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: root.sport.muscleOptions
                        currentIndex: root.sport.muscleIndex
                        Accessible.name: "Мышцы"
                        objectName: "sportMuscleFilter"
                        onActivated: root.sport.setMuscleFilter(currentText)
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    visible: root.filtersExpanded && !Qt.inputMethod.visible && (root.wide || !root.sport.hasSelection)
                    Layout.preferredHeight: implicitHeight
                    spacing: Theme.spaceSmall
                    AppButton {
                        text: "Избранное"
                        tone: root.sport.favoritesOnly ? "primary" : "secondary"
                        objectName: "sportFavoritesFilter"
                        onClicked: root.sport.favoritesOnly = !root.sport.favoritesOnly
                    }
                    AppButton {
                        text: "Хочу прогрессировать"
                        tone: root.sport.progressionOnly ? "primary" : "secondary"
                        onClicked: root.sport.progressionOnly = !root.sport.progressionOnly
                    }
                    AppButton {
                        text: "Без выхода на пол"
                        tone: root.sport.noFloor ? "primary" : "secondary"
                        onClicked: root.sport.noFloor = !root.sport.noFloor
                    }
                    AppButton {
                        text: "Без опоры на кисти"
                        tone: root.sport.noPalmSupport ? "primary" : "secondary"
                        onClicked: root.sport.noPalmSupport = !root.sport.noPalmSupport
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spaceLarge
                    Controls.ScrollView {
                        id: listScroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: root.wide ? Theme.cardWidth : -1
                        visible: root.wide || !root.sport.hasSelection
                        contentWidth: availableWidth
                        clip: true
                        ColumnLayout {
                            width: listScroll.availableWidth
                            spacing: Theme.spaceMedium
                            Repeater {
                                model: root.sport.exercises
                                SurfaceCard {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: Theme.spaceSmall
                                        AppButton {
                                            Layout.fillWidth: true
                                            text: modelData.title
                                            tone: root.details.identifier === modelData.identifier ? "primary" : "secondary"
                                            objectName: "sportExercise_" + modelData.identifier
                                            onClicked: root.sport.selectExercise(modelData.identifier)
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.equipment
                                            color: Theme.secondaryText
                                            font.pixelSize: Theme.fontSmall
                                            font.family: Theme.fontFamily
                                            wrapMode: Text.Wrap
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            visible: text.length > 0
                                            text: modelData.badges
                                            color: Theme.progressAccent
                                            font.pixelSize: Theme.fontSmall
                                            font.family: Theme.fontFamily
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                            EmptyState {
                                Layout.fillWidth: true
                                visible: root.sport.exercises.length === 0
                                title: "Нет подходящих подготовленных карточек"
                                description: "Измените поиск или фильтры. Непроверенные карточки не показываются как готовые."
                            }
                            AppButton {
                                text: "Сбросить поиск и фильтры"
                                tone: "secondary"
                                onClicked: root.sport.resetFilters()
                            }
                        }
                    }
                    Controls.ScrollView {
                        id: detailScroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 0
                        visible: root.sport.hasSelection
                        objectName: "sportDetails"
                        contentWidth: availableWidth
                        clip: true
                        ColumnLayout {
                            width: detailScroll.availableWidth
                            spacing: Theme.spaceLarge
                            AppButton {
                                visible: !root.wide
                                text: "К списку упражнений"
                                tone: "secondary"
                                objectName: "sportBackToList"
                                onClicked: root.sport.clearSelection()
                            }
                            SectionHeader {
                                Layout.fillWidth: true
                                title: root.details.title || ""
                                description: root.details.conditions || ""
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                spacing: Theme.spaceSmall
                                AppButton {
                                    text: root.details.favorite ? "В избранном" : "В избранное"
                                    enabled: root.sport.storageReady
                                    objectName: "sportFavorite"
                                    onClicked: root.sport.requestFavorite()
                                }
                                AppButton {
                                    text: "Хочу прогрессировать"
                                    tone: root.details.progression ? "primary" : "secondary"
                                    enabled: root.sport.storageReady
                                    objectName: "sportProgression"
                                    onClicked: root.sport.requestProgression()
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: !root.sport.storageReady
                                text: "Локальное сохранение отметок недоступно."
                                color: Theme.warning
                                font.pixelSize: Theme.fontBody
                                font.family: Theme.fontFamily
                                wrapMode: Text.Wrap
                            }
                            Repeater {
                                model: [
                                    { title: "Краткая техника", body: root.details.brief || "" },
                                    { title: "Мышцы", body: root.details.muscles || "" },
                                    { title: "Оборудование", body: root.details.equipment || "" },
                                    { title: "Подготовка", body: root.details.preparation || "" },
                                    { title: "Выполнение", body: root.details.steps || "" },
                                    { title: "Дыхание", body: root.details.breathing || "" },
                                    { title: "Частые ошибки", body: root.details.errors || "" },
                                    { title: "Ограничения и условия", body: root.details.limitations || "" },
                                    { title: "Как записывать", body: root.details.counting || "" }
                                ]
                                SurfaceCard {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: Theme.spaceSmall
                                        SectionHeader { Layout.fillWidth: true; title: modelData.title }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.body
                                            textFormat: Text.PlainText
                                            color: Theme.text
                                            font.pixelSize: Theme.fontBody
                                            font.family: Theme.fontFamily
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                            SectionHeader { Layout.fillWidth: true; title: "Другие варианты" }
                            Repeater {
                                model: root.details.alternatives || []
                                AppButton {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: modelData.title + (modelData.ready ? "" : " · готовится")
                                    enabled: modelData.ready
                                    tone: "secondary"
                                    onClicked: root.sport.selectExercise(modelData.identifier)
                                }
                            }
                            SectionHeader { Layout.fillWidth: true; title: "Источники" }
                            Repeater {
                                model: root.details.sources || []
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    AppButton {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        tone: "secondary"
                                        onClicked: Qt.openUrlExternally(modelData.url)
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: "Сверено: " + modelData.date
                                        color: Theme.secondaryText
                                        font.pixelSize: Theme.fontSmall
                                        font.family: Theme.fontFamily
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        EmptyState {
            Layout.fillWidth: true
            visible: root.sport.section === "history" || root.sport.section === "progress"
            title: root.sport.section === "history" ? "История занятий появится после записи тренировки" : "Динамика будет строиться по сохранённым занятиям"
            description: "Экран записи ещё готовится. Пропуски не превращаются в нули, а разные условия выполнения не объединяются молча."
        }
    }
}
