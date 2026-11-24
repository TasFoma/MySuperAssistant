import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Mobile 1.0

Item {
    id: root
    required property var mobile
    required property var client
    property bool keyboardVisible: false
    objectName: "mobileMoreScreen"
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceSmall
        AppButton {
            visible: root.mobile.moreSection !== MobileState.Menu && !root.keyboardVisible
            text: "Назад"
            tone: "secondary"
            onClicked: root.mobile.goBack()
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Controls.ScrollView {
                id: menu
                anchors.fill: parent
                visible: root.mobile.moreSection === MobileState.Menu
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: menu.availableWidth
                    spacing: Theme.spaceLarge
                    SectionHeader {
                        Layout.fillWidth: true
                        prominent: true
                        title: "Ещё"
                        description: "Ассистент и параметры подключения"
                    }
                    AppButton {
                        objectName: "mobileOpenAssistant"
                        Layout.fillWidth: true
                        text: "Ассистент"
                        icon.name: "assistant"
                        onClicked: root.mobile.showMore(MobileState.Assistant)
                    }
                    AppButton {
                        objectName: "mobileOpenSettings"
                        Layout.fillWidth: true
                        text: "Настройки"
                        icon.name: "settings"
                        tone: "secondary"
                        onClicked: root.mobile.showMore(MobileState.Settings)
                    }
                }
            }
            MobileChatScreen {
                anchors.fill: parent
                visible: root.mobile.moreSection === MobileState.Assistant
                client: root.client
                mobile: root.mobile
            }
            MobileSettingsScreen {
                anchors.fill: parent
                visible: root.mobile.moreSection === MobileState.Settings
                settings: root.mobile.settings
            }
        }
    }
}
