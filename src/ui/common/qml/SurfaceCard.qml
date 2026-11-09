import QtQuick
import QtQuick.Controls.Basic as Controls

Controls.Pane {
    id: control
    property color surfaceColor: Theme.surface
    padding: Theme.spaceLarge
    implicitWidth: Theme.cardWidth
    background: Rectangle {
        color: control.surfaceColor
        radius: Theme.radiusCard
        border.color: Theme.border
        border.width: Theme.borderWidth
    }
}

