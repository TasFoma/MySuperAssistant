import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0
import Msa.Ui.Desktop 1.0

Controls.ScrollView {
    id: root
    required property var desktop
    objectName: "todayScreen"
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceSection
        SectionHeader {
            prominent: true
            Layout.fillWidth: true
            title: "Сегодня"
            description: root.desktop.todayDate
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
                    objectName: "openPlansButton"
                    text: "Открыть планы"
                    onClicked: root.desktop.navigate(DesktopState.Plans)
                }
            }
        }
    }
}
