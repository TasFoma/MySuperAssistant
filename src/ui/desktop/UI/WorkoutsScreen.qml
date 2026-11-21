import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    objectName: "workoutsScreen"
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceSection
        SectionHeader {
            prominent: true
            Layout.fillWidth: true
            title: "Тренировки"
            description: "Движение в своём ритме"
        }
        EmptyState {
            Layout.fillWidth: true
            title: "Место для будущих тренировок"
            description: "Здесь появятся программы, упражнения и история занятий. Сейчас этот раздел готовится."
        }
    }
}
