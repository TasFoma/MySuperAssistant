import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Mobile 1.0

Controls.ScrollView {
    id: root
    required property var mobile
    objectName: "mobileTodayScreen"
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceSection
        SectionHeader {
            Layout.fillWidth: true
            prominent: true
            title: "Сегодня"
            description: root.mobile.todayDate
        }
        SurfaceCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.spaceLarge
                SectionHeader {
                    Layout.fillWidth: true
                    title: "Добро пожаловать"
                    description: "Найдите время для того, что важно сегодня."
                }
                AppButton {
                    objectName: "mobileOpenPlans"
                    Layout.fillWidth: true
                    text: "Открыть планы"
                    onClicked: root.mobile.navigate(MobileState.Plans)
                }
            }
        }
    }
}
