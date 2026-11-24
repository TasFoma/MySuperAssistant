import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    required property var sections
    required property var selectedSection
    signal navigationRequested(var section)
    spacing: Theme.spaceTiny
    implicitHeight: Theme.bottomNavigationHeight
    Repeater {
        model: root.sections
        AppButton {
            id: entry
            required property var modelData
            objectName: "mobileNavigation_" + modelData.key
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: Theme.controlHeight
            Layout.preferredHeight: Theme.bottomNavigationHeight
            padding: Theme.spaceSmall
            text: modelData.title
            tone: root.selectedSection === modelData.section ? "primary" : "secondary"
            contentItem: ColumnLayout {
                spacing: Theme.spaceTiny
                AppIcon {
                    Layout.alignment: Qt.AlignHCenter
                    name: entry.modelData.icon
                    ink: root.selectedSection === entry.modelData.section ? Theme.accentText : Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    text: entry.text
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    color: root.selectedSection === entry.modelData.section ? Theme.accentText : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            onClicked: root.navigationRequested(modelData.section)
        }
    }
}
