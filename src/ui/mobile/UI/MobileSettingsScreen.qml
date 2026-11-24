import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Flickable {
    id: root
    objectName: "mobileSettingsScroll"
    required property var settings
    clip: true
    contentWidth: width
    contentHeight: contents.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    Controls.ScrollBar.vertical: Controls.ScrollBar {}
    ColumnLayout {
        id: contents
        width: root.width
        spacing: Theme.spaceMedium
        SectionHeader {
            prominent: true
            Layout.fillWidth: true
            title: "Настройки"
            description: "Подключение к серверу"
        }
        AppTextField {
            id: host
            objectName: "mobileSettingsHost"
            Layout.fillWidth: true
            text: root.settings.host
            placeholderText: "Хост без схемы URL"
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
        }
        AppTextField {
            id: port
            objectName: "mobileSettingsPort"
            Layout.fillWidth: true
            text: root.settings.port
            placeholderText: "Порт"
            inputMethodHints: Qt.ImhDigitsOnly
        }
        AppTextField {
            id: token
            objectName: "mobileSettingsToken"
            Layout.fillWidth: true
            text: root.settings.token
            placeholderText: "Токен"
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
        }
        RowLayout {
            Layout.fillWidth: true
            Controls.CheckBox {
                id: autoConnect
                checked: root.settings.autoConnect
                palette.text: Theme.text
                palette.highlight: Theme.accent
                Accessible.name: "Подключаться при запуске"
            }
            Text {
                Layout.fillWidth: true
                text: "Подключаться при запуске"
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
            }
        }
        AppButton {
            objectName: "mobileSaveSettings"
            Layout.fillWidth: true
            text: "Сохранить"
            onClicked: root.settings.savePreferences(host.text, port.text, token.text, autoConnect.checked)
        }
        AppButton {
            objectName: "mobileConnectSettings"
            Layout.fillWidth: true
            text: "Подключиться"
            tone: "secondary"
            enabled: root.settings.canConnect
            onClicked: root.settings.connectToServer()
        }
        SectionHeader {
            Layout.fillWidth: true
            description: "Подключение использует сохранённые параметры. После обрыва запись не повторяется автоматически."
        }
        SectionHeader {
            Layout.fillWidth: true
            description: root.settings.settingsMessage
        }
    }
}
