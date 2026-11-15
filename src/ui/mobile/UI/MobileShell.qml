import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Rectangle {
    id: root
    required property var client
    required property var mobile
    property bool keyboardVisible: Qt.inputMethod.visible
    width: Theme.mobileWidth
    height: Theme.mobileHeight
    color: Theme.background
    focus: true
    Keys.onReleased: function(event) {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = root.mobile.goBack()
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceSmall + root.SafeArea.margins.left
        anchors.rightMargin: Theme.spaceSmall + root.SafeArea.margins.right
        anchors.topMargin: Theme.spaceSmall + root.SafeArea.margins.top
        anchors.bottomMargin: Theme.spaceSmall + root.SafeArea.margins.bottom
        spacing: Theme.spaceSmall
        ConnectionBanner {
            objectName: "mobileConnectionBanner"
            Layout.fillWidth: true
            Layout.minimumHeight: implicitHeight
            Layout.maximumHeight: implicitHeight
            visible: !root.keyboardVisible
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
        StackLayout {
            objectName: "mobilePageArea"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.controlHeight
            currentIndex: root.mobile.page
            clip: true
            MobileChatScreen { client: root.client; mobile: root.mobile }
            MobilePlansScreen { client: root.client; mobile: root.mobile }
            MobileRemindersScreen { client: root.client; editor: root.mobile.planner }
            MobileSettingsScreen { settings: root.mobile.settings }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !root.keyboardVisible
            spacing: Theme.spaceTiny
            Repeater {
                model: root.mobile.pages
                AppButton {
                    required property int index
                    required property string modelData
                    objectName: "mobileNavigation" + index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 0
                    padding: Theme.spaceTiny
                    text: modelData
                    tone: root.mobile.page === index ? "primary" : "secondary"
                    contentItem: Text {
                        text: parent.text
                        color: root.mobile.page === parent.index ? Theme.accentText : Theme.text
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                    onClicked: root.mobile.navigate(index)
                }
            }
        }
    }
    PlannerFormDialog { editor: root.mobile.planner }
}
