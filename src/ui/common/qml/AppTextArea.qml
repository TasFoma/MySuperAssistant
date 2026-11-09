import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.TextArea {
    id: control
    property bool hasError: false
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    color: enabled ? Theme.text : Theme.disabledText
    placeholderTextColor: Theme.secondaryText
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    textFormat: TextEdit.PlainText
    wrapMode: TextEdit.Wrap
    selectByMouse: true
    padding: Theme.spaceMedium
    implicitWidth: Theme.inputWidth
    implicitHeight: Math.max(Theme.multilineHeight, contentHeight + topPadding + bottomPadding)
    Accessible.name: placeholderText
    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.raised
        border.width: control.activeFocus ? Theme.focusWidth : Theme.borderWidth
        border.color: control.hasError ? Theme.danger : control.activeFocus ? Theme.accent : Theme.border
    }
}

