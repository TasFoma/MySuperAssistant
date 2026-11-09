import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Msa.Ui.Common 1.0

Controls.ScrollView {
    id: root
    objectName: "settingsScroll"
    required property var desktop
    clip: true
    contentWidth: availableWidth
    FolderDialog {
        id: voiceFolderDialog
        title: "Папка модели Vosk"
        onAccepted: root.desktop.voice.chooseModel(selectedFolder.toString())
    }
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceLarge
        SectionHeader { Layout.fillWidth: true; title: "Настройки"; description: "Подключение к серверу" }
        SurfaceCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.spaceMedium
                SectionHeader { Layout.fillWidth: true; title: "Сервер"; description: "Адрес без схемы URL, порт и токен авторизации" }
                AppTextField { id: host; objectName: "settingsHost"; Layout.fillWidth: true; placeholderText: "Хост"; text: root.desktop.host }
                AppTextField { id: port; objectName: "settingsPort"; Layout.fillWidth: true; placeholderText: "Порт"; text: root.desktop.port; inputMethodHints: Qt.ImhDigitsOnly }
                AppTextField {
                    id: token
                    objectName: "settingsToken"
                    Layout.fillWidth: true
                    placeholderText: "Токен"
                    text: root.desktop.token
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                }
                RowLayout {
                    Layout.fillWidth: true
                    Controls.CheckBox {
                        id: autoConnect
                        objectName: "autoConnectCheck"
                        checked: root.desktop.autoConnect
                        palette.highlight: Theme.accent
                        palette.base: Theme.raised
                        palette.text: Theme.text
                        Accessible.name: "Подключаться при запуске"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Подключаться при запуске"
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        color: Theme.text
                    }
                }
                SectionHeader {
                    Layout.fillWidth: true
                    description: "Можно выключить для работы без сервера. Автоматическое подключение ограничено пятью попытками."
                }
                RowLayout {
                    spacing: Theme.spaceSmall
                    AppButton {
                        objectName: "saveSettingsButton"
                        text: "Сохранить"
                        onClicked: root.desktop.savePreferences(host.text, port.text, token.text, autoConnect.checked)
                    }
                    AppButton {
                        objectName: "connectSettingsButton"
                        text: "Подключиться"
                        tone: "secondary"
                        enabled: root.desktop.canConnect
                        onClicked: root.desktop.connectToServer()
                    }
                }
                SectionHeader { Layout.fillWidth: true; description: "Подключение использует сохранённые параметры. Переключатель выше действует при следующем запуске." }
                SectionHeader { Layout.fillWidth: true; description: root.desktop.settingsMessage }
            }
        }
        SurfaceCard {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.spaceMedium
                SectionHeader { Layout.fillWidth: true; title: "Звук и голосовой ввод"; description: "Устройства сохраняются сразу. Запись включается только кнопкой в чате." }
                SectionHeader { Layout.fillWidth: true; description: "Микрофон" }
                Controls.ComboBox {
                    objectName: "inputDeviceCombo"
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeight
                    model: root.desktop.audioDevices.inputNames
                    currentIndex: root.desktop.audioDevices.inputIndex
                    onActivated: index => root.desktop.audioDevices.selectInput(index)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    palette.button: Theme.raised
                    palette.buttonText: Theme.text
                    palette.text: Theme.text
                    palette.base: Theme.surface
                    palette.highlight: Theme.accent
                    palette.highlightedText: Theme.accentText
                    Accessible.name: "Микрофон для диктовки"
                }
                SectionHeader { Layout.fillWidth: true; description: "Динамики или наушники — музыка и озвучка" }
                Controls.ComboBox {
                    objectName: "outputDeviceCombo"
                    Layout.fillWidth: true
                    implicitHeight: Theme.controlHeight
                    model: root.desktop.audioDevices.outputNames
                    currentIndex: root.desktop.audioDevices.outputIndex
                    onActivated: index => root.desktop.audioDevices.selectOutput(index)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    palette.button: Theme.raised
                    palette.buttonText: Theme.text
                    palette.text: Theme.text
                    palette.base: Theme.surface
                    palette.highlight: Theme.accent
                    palette.highlightedText: Theme.accentText
                    Accessible.name: "Устройство вывода музыки и речи"
                }
                AppButton {
                    objectName: "refreshAudioDevicesButton"
                    text: "Обновить устройства"
                    tone: "secondary"
                    onClicked: root.desktop.audioDevices.refresh()
                }
                SectionHeader { Layout.fillWidth: true; description: root.desktop.audioDevices.errorText }
                Text {
                    Layout.fillWidth: true
                    text: root.desktop.voice.modelPath
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    color: Theme.secondaryText
                }
                AppButton {
                    objectName: "voiceModelButton"
                    text: "Выбрать модель Vosk…"
                    tone: "secondary"
                    onClicked: voiceFolderDialog.open()
                }
                SectionHeader { Layout.fillWidth: true; description: root.desktop.voice.errorText }
            }
        }
    }
}
