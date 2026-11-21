import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

ColumnLayout {
    id: root
    required property var devices
    spacing: Theme.spaceSmall
    SectionHeader {
        Layout.fillWidth: true
        description: "Динамики или наушники — музыка и озвучка"
    }
    Controls.ComboBox {
        id: choice
        objectName: "outputDeviceCombo"
        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        model: root.devices.outputNames
        enabled: root.devices.outputNames.length > 0
        displayText: currentText.length > 0 ? currentText : "Нет доступных устройств"
        currentIndex: root.devices.outputIndex
        onActivated: index => root.devices.selectOutput(index)
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        palette.button: Theme.raised
        palette.buttonText: Theme.text
        palette.text: Theme.text
        palette.base: Theme.surface
        palette.highlight: Theme.accent
        palette.highlightedText: Theme.accentText
        palette.dark: Theme.secondaryText
        palette.disabled.buttonText: Theme.disabledText
        palette.disabled.text: Theme.disabledText
        Accessible.name: "Устройство вывода музыки и речи"
        background: Rectangle {
            color: choice.enabled ? Theme.raised : Theme.disabledSurface
            radius: Theme.radiusControl
            border.width: choice.visualFocus ? Theme.focusWidth : Theme.borderWidth
            border.color: choice.visualFocus ? Theme.focusBorder : Theme.border
        }
    }
}
