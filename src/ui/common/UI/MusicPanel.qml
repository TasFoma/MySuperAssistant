import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

SurfaceCard {
    id: root
    required property var music
    property bool compact: false
    signal openMusicRequested
    objectName: "musicPanel"
    padding: compact ? Theme.spaceMedium : Theme.spaceLarge
    implicitHeight: compact ? Theme.miniPlayerHeight : content.implicitHeight + topPadding + bottomPadding
    GridLayout {
        id: content
        anchors.fill: parent
        columns: root.compact ? 3 : 1
        columnSpacing: Theme.spaceMedium
        rowSpacing: Theme.spaceMedium
        ColumnLayout {
            Layout.row: 0
            Layout.column: 0
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.spaceTiny
            Text {
                objectName: "musicTrackTitle"
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
                text: root.compact ? (root.music.errorText.length > 0 ? "Ошибка: " + root.music.errorText : root.music.scanning ? "Поиск треков…" : root.music.playing ? "Воспроизведение" : root.music.canControl ? "На паузе" : "Нет треков") : root.music.folder
                textFormat: Text.PlainText
                visible: text.length > 0
                elide: Text.ElideMiddle
                color: root.compact && root.music.errorText.length > 0 ? Theme.danger : Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                HoverHandler {
                    id: statusHover
                }
                ToolTip.visible: statusHover.hovered && root.music.errorText.length > 0
                ToolTip.text: root.music.errorText
                Accessible.name: text
            }
        }
        RowLayout {
            Layout.row: root.compact ? 0 : 1
            Layout.column: root.compact ? 1 : 0
            spacing: Theme.spaceSmall
            AppButton {
                text: root.compact ? "" : "Назад"
                icon.name: root.compact ? "previous" : ""
                padding: root.compact ? Theme.spaceMedium : Theme.spaceLarge
                tone: "secondary"
                objectName: "musicPreviousButton"
                enabled: root.music.canControl
                Accessible.name: "Предыдущий трек"
                onClicked: root.music.previous()
            }
            AppButton {
                text: root.compact ? "" : root.music.playing ? "Пауза" : "Играть"
                icon.name: root.compact ? (root.music.playing ? "pause" : "play") : ""
                padding: root.compact ? Theme.spaceMedium : Theme.spaceLarge
                objectName: "musicPlayButton"
                enabled: root.music.canControl
                Accessible.name: root.music.playing ? "Пауза" : "Воспроизвести"
                onClicked: root.music.playPause()
            }
            AppButton {
                text: root.compact ? "" : "Дальше"
                icon.name: root.compact ? "next" : ""
                padding: root.compact ? Theme.spaceMedium : Theme.spaceLarge
                tone: "secondary"
                objectName: "musicNextButton"
                enabled: root.music.canControl
                Accessible.name: "Следующий трек"
                onClicked: root.music.next()
            }
            AppButton {
                text: "Перемешать"
                tone: "secondary"
                objectName: "musicShuffleButton"
                visible: !root.compact
                enabled: root.music.folder.length > 0 && !root.music.scanning
                onClicked: root.music.reshuffle()
            }
            AppButton {
                text: "Папка…"
                tone: "secondary"
                objectName: "musicFolderButton"
                visible: !root.compact
                onClicked: folderDialog.open()
            }
        }
        RowLayout {
            Layout.row: root.compact ? 0 : 2
            Layout.column: root.compact ? 2 : 0
            spacing: Theme.spaceSmall
            Slider {
                id: volumeSlider
                objectName: "musicVolumeSlider"
                Layout.preferredWidth: root.compact ? Theme.miniVolumeWidth : Theme.musicVolumeWidth
                implicitHeight: Theme.controlHeight
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
                visible: !root.compact
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            Text {
                visible: !root.compact
                text: root.music.positionText
                color: Theme.secondaryText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            AppButton {
                visible: root.compact
                objectName: "openMusicButton"
                icon.name: "music"
                padding: Theme.spaceMedium
                tone: "secondary"
                Accessible.name: "Открыть музыку"
                onClicked: root.openMusicRequested()
            }
        }
        Text {
            Layout.row: 3
            Layout.column: 0
            Layout.fillWidth: true
            text: "Ошибка: " + root.music.errorText
            textFormat: Text.PlainText
            visible: !root.compact && root.music.errorText.length > 0
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
