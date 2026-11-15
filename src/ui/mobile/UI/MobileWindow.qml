import QtQuick
import QtQuick.Window
import Msa.Ui.Common 1.0

Window {
    id: mobileWindow
    required property var client
    required property var mobile
    objectName: "mobileWindow"
    title: "MySuperAssistant"
    width: Theme.mobileWidth
    height: Theme.mobileHeight
    minimumWidth: Theme.mobileMinimumWidth
    color: Theme.background
    visible: true
    MobileShell {
        anchors.fill: parent
        client: mobileWindow.client
        mobile: mobileWindow.mobile
    }
}
