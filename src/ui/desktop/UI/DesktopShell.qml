import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Desktop 1.0

Rectangle {
    id: root
    required property var client
    required property var desktop
    width: Theme.desktopWidth
    height: Theme.desktopHeight
    color: Theme.background
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceLarge
        spacing: Theme.spaceLarge
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spaceLarge
            SurfaceCard {
                objectName: "desktopSidebar"
                Layout.preferredWidth: Theme.navigationWidth
                Layout.minimumWidth: Theme.navigationWidth
                Layout.maximumWidth: Theme.navigationWidth
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSmall
                    SectionHeader {
                        Layout.fillWidth: true
                        title: "MySuperAssistant"
                        description: "Время для важного"
                    }
                    Item {
                        implicitHeight: Theme.spaceLarge
                    }
                    Repeater {
                        model: root.desktop.sections.filter(entry => entry.section !== DesktopState.Settings)
                        AppButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.title
                            icon.name: modelData.icon
                            objectName: "navigation_" + modelData.key
                            tone: root.desktop.page === modelData.section ? "primary" : "secondary"
                            onClicked: root.desktop.navigate(modelData.section)
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                    AppButton {
                        Layout.fillWidth: true
                        text: "Обновить данные"
                        tone: "secondary"
                        enabled: root.client.serverActionsEnabled
                        onClicked: root.client.refresh()
                    }
                    AppButton {
                        Layout.fillWidth: true
                        text: "Настройки"
                        icon.name: "settings"
                        objectName: "navigation_settings"
                        tone: root.desktop.page === DesktopState.Settings ? "primary" : "secondary"
                        onClicked: root.desktop.navigate(DesktopState.Settings)
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.fillHeight: true
                spacing: Theme.spaceMedium
                ConnectionBanner {
                    objectName: "desktopConnectionBanner"
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
                    objectName: "desktopPageArea"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    TodayScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Today
                        desktop: root.desktop
                    }
                    PlansScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Plans
                        client: root.client
                        desktop: root.desktop
                    }
                    WorkoutsScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Workouts
                    }
                    MusicScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Music
                        desktop: root.desktop
                    }
                    ChatScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Assistant
                        client: root.client
                        voice: root.desktop.voice
                    }
                    SettingsScreen {
                        anchors.fill: parent
                        visible: root.desktop.page === DesktopState.Settings
                        desktop: root.desktop
                    }
                }
            }
        }
        MusicPanel {
            Layout.fillWidth: true
            Layout.minimumHeight: Theme.miniPlayerHeight
            Layout.maximumHeight: Theme.miniPlayerHeight
            compact: true
            music: root.desktop.music
            onOpenMusicRequested: root.desktop.navigate(DesktopState.Music)
        }
    }
    PlannerDialog {
        editor: root.desktop.planner
    }
}
