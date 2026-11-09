import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.Popup {
    id: plannerPopup
    required property var editor
    objectName: "plannerDialog"
    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Controls.Popup.NoAutoClose
    padding: Theme.spaceLarge
    width: Math.min(Theme.formWidth, parent ? parent.width - Theme.spacePage : Theme.formWidth)
    height: Math.min(contents.implicitHeight + topPadding + bottomPadding,
                     parent ? parent.height - Theme.spacePage : Theme.desktopHeight)
    background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.radiusCard }
    Connections {
        target: plannerPopup.editor
        function onFormChanged() {
            if (plannerPopup.editor.active) { plannerPopup.open() }
            else { plannerPopup.close() }
        }
    }
    Component.onCompleted: { if (editor.active) open() }
    contentItem: Controls.ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            id: contents
            width: scroll.availableWidth
            spacing: Theme.spaceMedium
            SectionHeader { Layout.fillWidth: true; title: plannerPopup.editor.heading; description: plannerPopup.editor.explanation }
            Repeater {
                model: plannerPopup.editor.fields
                ColumnLayout {
                    id: field
                    required property var modelData
                    visible: plannerPopup.editor.fieldVisibility[modelData.key]
                    Layout.fillWidth: true
                    spacing: Theme.spaceTiny
                    SectionHeader { Layout.fillWidth: true; description: field.modelData.label }
                    AppTextField {
                        Layout.fillWidth: true
                        objectName: "plannerField_" + field.modelData.key
                        visible: field.modelData.options.length === 0 && !field.modelData.multiline
                        text: field.modelData.value
                        readOnly: !plannerPopup.editor.editable
                        Accessible.name: field.modelData.label
                        onTextEdited: plannerPopup.editor.setField(field.modelData.key, text)
                    }
                    AppTextArea {
                        Layout.fillWidth: true
                        visible: field.modelData.multiline
                        text: field.modelData.value
                        readOnly: !plannerPopup.editor.editable
                        Accessible.name: field.modelData.label
                        onTextChanged: { if (visible) plannerPopup.editor.setField(field.modelData.key, text) }
                    }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        objectName: "plannerChoice_" + field.modelData.key
                        visible: field.modelData.options.length > 0
                        model: field.modelData.options
                        currentIndex: field.modelData.optionIndex
                        enabled: plannerPopup.editor.editable
                        Accessible.name: field.modelData.label
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        palette.button: Theme.raised
                        palette.buttonText: Theme.text
                        palette.base: Theme.surface
                        palette.text: Theme.text
                        palette.highlight: Theme.accent
                        palette.highlightedText: Theme.accentText
                        onActivated: function(index) { plannerPopup.editor.setChoice(field.modelData.key, index) }
                    }
                }
            }
            ErrorBanner { Layout.fillWidth: true; message: plannerPopup.editor.errorText }
            SectionHeader { Layout.fillWidth: true; visible: plannerPopup.editor.pending; description: "Ждём подтверждения сервера…" }
            RowLayout {
                AppButton {
                    objectName: "plannerSubmitButton"
                    text: plannerPopup.editor.destructive ? "Удалить" : "Подтвердить"
                    tone: plannerPopup.editor.destructive ? "danger" : "primary"
                    enabled: plannerPopup.editor.canSubmit
                    onClicked: plannerPopup.editor.submit()
                }
                AppButton {
                    objectName: "plannerCancelButton"
                    text: "Отмена"
                    tone: "secondary"
                    enabled: !plannerPopup.editor.pending
                    onClicked: plannerPopup.editor.cancel()
                }
            }
        }
    }
}
