import QtQuick
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Mobile 1.0

Rectangle {
    id: root
    required property var client
    required property var mobile
    property bool keyboardVisible: Qt.inputMethod.visible
    property real safeTop: root.SafeArea.margins.top
    property real safeBottom: root.SafeArea.margins.bottom
    property real safeLeft: root.SafeArea.margins.left
    property real safeRight: root.SafeArea.margins.right
    width: Theme.mobileWidth
    height: Theme.mobileHeight
    color: Theme.background
    focus: true
    Keys.onReleased: function (event) {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            if (Qt.inputMethod.visible) {
                Qt.inputMethod.hide();
                event.accepted = true;
            } else {
                event.accepted = root.mobile.goBack();
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceSmall + root.safeLeft
        anchors.rightMargin: Theme.spaceSmall + root.safeRight
        anchors.topMargin: Theme.spaceSmall + root.safeTop
        anchors.bottomMargin: Theme.spaceSmall + root.safeBottom
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
        Item {
            objectName: "mobilePageArea"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.controlHeight
            clip: true
            MobileTodayScreen {
                anchors.fill: parent
                visible: root.mobile.page === MobileState.Today
                mobile: root.mobile
            }
            MobilePlansScreen {
                anchors.fill: parent
                visible: root.mobile.page === MobileState.Plans
                client: root.client
                mobile: root.mobile
            }
            MobileSportScreen {
                mobile: root.mobile
                anchors.fill: parent
                visible: root.mobile.page === MobileState.Sport
            }
            MobileMoreScreen {
                keyboardVisible: root.keyboardVisible
                anchors.fill: parent
                visible: root.mobile.page === MobileState.More
                client: root.client
                mobile: root.mobile
            }
        }
        AppBottomNavigation {
            objectName: "mobileBottomNavigation"
            Layout.fillWidth: true
            Layout.minimumHeight: Theme.bottomNavigationHeight
            Layout.maximumHeight: Theme.bottomNavigationHeight
            visible: !root.keyboardVisible
            sections: root.mobile.sections
            selectedSection: root.mobile.page
            onNavigationRequested: function (section) {
                root.mobile.navigate(section);
            }
        }
    }
    PlannerFormDialog {
        editor: root.mobile.planner
        confirmDiscard: true
        safeTop: root.safeTop
        safeBottom: root.safeBottom
        safeLeft: root.safeLeft
        safeRight: root.safeRight
    }
}
