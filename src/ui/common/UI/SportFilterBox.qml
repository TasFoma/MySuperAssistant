import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.ComboBox {
    id: control
    implicitHeight: Theme.controlHeight
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    palette.button: Theme.raised
    palette.buttonText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.mid: Theme.secondaryText
    palette.dark: Theme.secondaryText
    palette.light: Theme.focusBorder
    background: Rectangle {
        color: Theme.raised
        radius: Theme.radiusControl
        border.color: control.activeFocus ? Theme.focusBorder : Theme.border
        border.width: control.activeFocus ? Theme.focusWidth : Theme.borderWidth
    }
}
