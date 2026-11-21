import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    required property var desktop
    objectName: "musicScreen"
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceSection
        SectionHeader {
            prominent: true
            Layout.fillWidth: true
            title: "Музыка"
            description: "Ваша библиотека — доступна и без сервера"
        }
        MusicPanel {
            objectName: "fullMusicPanel"
            Layout.fillWidth: true
            music: root.desktop.music
        }
        SurfaceCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.spaceMedium
                AudioOutputSelector {
                    Layout.fillWidth: true
                    devices: root.desktop.audioDevices
                }
                AppButton {
                    text: "Обновить устройства"
                    tone: "secondary"
                    onClicked: root.desktop.audioDevices.refresh()
                }
                ErrorBanner {
                    Layout.fillWidth: true
                    message: root.desktop.audioDevices.errorText
                }
            }
        }
    }
}
