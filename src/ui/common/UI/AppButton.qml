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
        leftPadding: control.icon.name.length > 0 ? Theme.iconSize + Theme.spaceSmall : 0
        horizontalAlignment: control.icon.name.length > 0 ? Text.AlignLeft : Text.AlignHCenter
        AppIcon {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            name: control.icon.name
            ink: parent.color
            visible: name.length > 0
        }
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        property color restingColor: control.tone === "primary"
                                     ? (control.hovered ? Theme.accentHover : Theme.accent)
                                     : (control.hovered ? Theme.hover : control.tone === "danger" ? Theme.dangerSurface : Theme.raised)
        radius: Theme.radiusControl
        color: !control.enabled ? Theme.disabledSurface : control.tone === "primary"
               ? (control.down ? Theme.accentPressed : restingColor)
               : (control.down ? Theme.pressed : restingColor)
        border.width: control.visualFocus ? Theme.focusWidth : Theme.borderWidth
        border.color: control.visualFocus ? Theme.focusBorder
                      : control.tone === "primary" && control.enabled ? Theme.accent : Theme.border
        Behavior on restingColor {
            enabled: control.enabled
            ColorAnimation { duration: Theme.shortMotion }
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.spaceTiny
            visible: control.visualFocus
            radius: Theme.radiusControl + Theme.spaceTiny
            color: "transparent"
            border.width: Theme.focusWidth
            border.color: Theme.focusBorder
        }
    }
}

