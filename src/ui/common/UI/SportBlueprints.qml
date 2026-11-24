import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var sport
    spacing: Theme.spaceMedium
    Repeater {
        model: root.sport.journal.blueprints
        SurfaceCard {
            id: programCard
            required property var modelData
            objectName: "sportBlueprint_" + modelData.identifier
            property bool expanded: false
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceMedium
                SectionHeader { Layout.fillWidth: true; title: programCard.modelData.title; description: programCard.modelData.description }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spaceSmall
                    AppButton { text: "Состав и условия"; objectName: "sportBlueprintDetails"; tone: "secondary"; onClicked: programCard.expanded = !programCard.expanded }
                    AppButton { text: "Создать свою копию"; enabled: root.sport.storageReady; onClicked: root.sport.journal.copyBlueprint(programCard.modelData.identifier) }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: programCard.expanded
                    spacing: Theme.spaceMedium
                    SectionHeader { Layout.fillWidth: true; title: "Оборудование"; description: programCard.modelData.equipment }
                    SectionHeader { Layout.fillWidth: true; title: "Условия"; description: programCard.modelData.conditions }
                    Repeater {
                        model: programCard.modelData.sessions
                        ColumnLayout {
                            id: sessionBlock
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.spaceSmall
                            SectionHeader { Layout.fillWidth: true; title: sessionBlock.modelData.title; description: sessionBlock.modelData.description }
                            Repeater {
                                model: sessionBlock.modelData.items
                                ColumnLayout {
                                    id: itemBlock
                                    required property var modelData
                                    Layout.fillWidth: true
                                    AppButton { Layout.fillWidth: true; text: itemBlock.modelData.title; tone: "secondary"; onClicked: root.sport.selectExercise(itemBlock.modelData.identifier) }
                                    SectionHeader { Layout.fillWidth: true; description: itemBlock.modelData.instructions }
                                    Repeater {
                                        model: itemBlock.modelData.alternatives
                                        SectionHeader { required property var modelData; Layout.fillWidth: true; description: "Альтернатива: " + modelData.title + ". " + modelData.description }
                                    }
                                }
                            }
                        }
                    }
                    SectionHeader { Layout.fillWidth: true; description: programCard.modelData.authorship }
                }
            }
        }
    }
}
