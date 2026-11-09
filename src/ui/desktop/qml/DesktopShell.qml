import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Rectangle {
    id: root
    required property var client
    required property var desktop
    width: Theme.desktopWidth
    height: Theme.desktopHeight
    color: Theme.background
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceLarge
        spacing: Theme.spaceLarge
        SurfaceCard {
            Layout.preferredWidth: Theme.navigationWidth
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceMedium
                SectionHeader { Layout.fillWidth: true; title: "Ассистент" }
                Repeater {
                    model: root.desktop.pages
                    AppButton {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        objectName: "navigation" + index
                        tone: root.desktop.page === index ? "primary" : "secondary"
                        onClicked: root.desktop.navigate(index)
                    }
                }
                Item { Layout.fillHeight: true }
                AppButton {
                    Layout.fillWidth: true
                    text: "Обновить"
                    tone: "secondary"
                    enabled: root.client.serverActionsEnabled
                    onClicked: root.client.refresh()
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            spacing: Theme.spaceMedium
            ConnectionBanner {
                Layout.fillWidth: true
                statusText: root.client.statusText
                online: root.client.online
                reconnectAvailable: root.client.reconnectAvailable
                staleData: !root.client.online && (root.client.tasks.count > 0 || root.client.messages.count > 0)
                onReconnectRequested: root.client.reconnect()
            }
            ErrorBanner {
                Layout.fillWidth: true
                message: root.client.errorText
                actionText: "Закрыть"
                actionEnabled: true
                onActionRequested: root.client.clearError()
            }
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ChatScreen { anchors.fill: parent; visible: root.desktop.page === 0; client: root.client }
                PlansScreen { anchors.fill: parent; visible: root.desktop.page === 1; client: root.client; desktop: root.desktop }
                RemindersScreen { anchors.fill: parent; visible: root.desktop.page === 2; client: root.client }
                SettingsScreen { anchors.fill: parent; visible: root.desktop.page === 3; desktop: root.desktop }
            }
        }
    }
}
