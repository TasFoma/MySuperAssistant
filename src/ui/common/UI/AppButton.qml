import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.Button {
    id: control
    property string tone: "primary"
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    font.weight: Theme.fontWeightStrong
    hoverEnabled: true
    padding: Theme.spaceLarge
    topPadding: Theme.spaceSmall
    bottomPadding: Theme.spaceSmall
    implicitHeight: Math.max(Theme.controlHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    Accessible.name: text
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: !control.enabled ? Theme.disabledText : control.tone === "primary" ? Theme.accentText : control.tone === "danger" ? Theme.danger : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.radiusControl
        color: !control.enabled ? Theme.raised : control.tone === "primary"
               ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
               : (control.down ? Theme.pressed : control.hovered ? Theme.hover : control.tone === "danger" ? Theme.dangerSurface : Theme.raised)
        border.width: control.visualFocus ? Theme.focusWidth : Theme.borderWidth
        border.color: control.visualFocus ? Theme.accent : control.tone === "primary" && control.enabled ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.shortMotion } }
    }
}

