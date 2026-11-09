import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

SurfaceCard {
    id: root
    required property var music
    objectName: "musicPanel"
    implicitHeight: content.implicitHeight + Theme.spaceLarge * 2
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: Theme.spaceSmall
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceMedium
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.spaceTiny
                Text {
                    Layout.fillWidth: true
                    text: root.music.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Theme.fontWeightStrong
                }
                Text {
                    Layout.fillWidth: true
                    text: root.music.folder
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    elide: Text.ElideMiddle
                    color: Theme.secondaryText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            Text {
                text: root.music.positionText
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            AppButton {
                text: "Папка…"
                tone: "secondary"
                objectName: "musicFolderButton"
                onClicked: folderDialog.open()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            AppButton {
                text: "Назад"
                tone: "secondary"
                objectName: "musicPreviousButton"
                enabled: root.music.canControl
                onClicked: root.music.previous()
            }
            AppButton {
                text: root.music.playing ? "Пауза" : "Играть"
                objectName: "musicPlayButton"
                enabled: root.music.canControl
                onClicked: root.music.playPause()
            }
            AppButton {
                text: "Дальше"
                tone: "secondary"
                objectName: "musicNextButton"
                enabled: root.music.canControl
                onClicked: root.music.next()
            }
            AppButton {
                text: "Перемешать"
                tone: "secondary"
                objectName: "musicShuffleButton"
                enabled: root.music.folder.length > 0 && !root.music.scanning
                onClicked: root.music.reshuffle()
            }
            Item { Layout.fillWidth: true }
            Slider {
                id: volumeSlider
                objectName: "musicVolumeSlider"
                Layout.preferredWidth: Theme.musicVolumeWidth
                from: 0
                to: 100
                value: root.music.volumePercent
                stepSize: 1
                Accessible.name: "Громкость музыки"
                onMoved: root.music.volumePercent = value
                background: Rectangle {
                    x: volumeSlider.leftPadding
                    y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                    width: volumeSlider.availableWidth
                    height: Theme.progressHeight
                    radius: Theme.progressHeight / 2
                    color: Theme.raised
                    Rectangle {
                        width: volumeSlider.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                    }
                }
                handle: Rectangle {
                    x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                    y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                    implicitWidth: Theme.indicatorSize
                    implicitHeight: Theme.indicatorSize
                    radius: width / 2
                    color: volumeSlider.pressed ? Theme.accentPressed : Theme.accent
                    border.width: volumeSlider.visualFocus ? Theme.focusWidth : 0
                    border.color: Theme.text
                }
            }
            Text {
                text: root.music.volumeText
                Layout.preferredWidth: Theme.controlHeight
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.music.errorText
            textFormat: Text.PlainText
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
    FolderDialog {
        id: folderDialog
        title: "Папка с музыкой"
        onAccepted: root.music.chooseFolder(selectedFolder.toString())
    }
}
