import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.TextField {
    id: control
    property bool hasError: false
    property string errorText: "Проверьте введённое значение."
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    color: enabled ? Theme.text : Theme.disabledText
    placeholderTextColor: enabled ? Theme.secondaryText : Theme.disabledText
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    selectByMouse: true
    leftPadding: Theme.spaceMedium
    rightPadding: Theme.spaceMedium
    topPadding: Theme.spaceSmall
    bottomPadding: Theme.spaceSmall + (hasError ? errorCaption.implicitHeight + Theme.spaceSmall : 0)
    implicitWidth: Theme.inputWidth
    implicitHeight: Theme.controlHeight + (hasError ? errorCaption.implicitHeight + Theme.spaceSmall : 0)
    Accessible.name: placeholderText
    Accessible.description: hasError ? errorCaption.text : ""
    background: Rectangle {
        radius: Theme.radiusControl
        color: control.enabled ? Theme.raised : Theme.disabledSurface
        border.width: control.hasError ? Theme.focusWidth : Theme.borderWidth
        border.color: control.hasError ? Theme.danger : control.activeFocus ? Theme.accent : Theme.border
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.spaceTiny
            visible: control.activeFocus
            radius: Theme.radiusControl + Theme.spaceTiny
            color: "transparent"
            border.width: Theme.focusWidth
            border.color: Theme.focusBorder
        }
    }
    Text {
        id: errorCaption
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceMedium
        anchors.rightMargin: Theme.spaceMedium
        anchors.bottomMargin: Theme.spaceSmall
        visible: control.hasError
        text: "Ошибка: " + (control.errorText.trim().length > 0 ? control.errorText : "Проверьте введённое значение.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: control.enabled ? Theme.danger : Theme.disabledText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSmall
    }
}

