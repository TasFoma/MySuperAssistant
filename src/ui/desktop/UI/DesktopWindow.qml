import QtQuick
import QtQuick.Window
import Msa.Ui.Common 1.0

Window {
    id: desktopWindow
    required property var client
    required property var desktop
    objectName: "desktopWindow"
    title: "MySuperAssistant"
    width: Theme.desktopWidth
    height: Theme.desktopHeight
    minimumWidth: Theme.desktopMinimumWidth
    minimumHeight: Theme.desktopMinimumHeight
    color: Theme.background
    visible: true
    DesktopShell {
        anchors.fill: parent
        client: desktopWindow.client
        desktop: desktopWindow.desktop
    }
}
