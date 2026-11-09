import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Item {
    id: root
    required property var client
    required property var voice
    RowLayout {
        anchors.fill: parent
        spacing: Theme.spaceLarge
        ColumnLayout {
            Layout.preferredWidth: Theme.conversationWidth
            Layout.minimumWidth: Theme.conversationWidth
            Layout.maximumWidth: Theme.conversationWidth
            Layout.fillHeight: true
            SectionHeader { Layout.fillWidth: true; title: "Беседы" }
            AppButton {
                Layout.fillWidth: true
                text: "Новая беседа"
                enabled: root.client.serverActionsEnabled
                onClicked: root.client.createChat()
            }
            ListView {
                id: chats
                objectName: "chatList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.spaceSmall
                model: root.client.chats
                Controls.ScrollBar.vertical: Controls.ScrollBar { }
                delegate: AppButton {
                    required property string identifier
                    required property string title
                    width: chats.width
                    text: title
                    tone: root.client.selectedChatIdentifier === identifier ? "primary" : "secondary"
                    onClicked: root.client.selectChat(identifier)
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            SectionHeader { Layout.fillWidth: true; title: "Чат"; description: "Сообщения и текстовые команды ассистенту" }
            EmptyState {
                Layout.fillWidth: true
                visible: root.client.selectedChatIdentifier.length === 0
                title: "Выберите беседу"
                description: "Загруженные сообщения можно читать без сервера."
            }
            ListView {
                id: messages
                objectName: "messageList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root.client.selectedChatIdentifier.length > 0 ? Theme.controlHeight * 2 : 0
                clip: true
                spacing: Theme.spaceMedium
                model: root.client.messages
                Controls.ScrollBar.vertical: Controls.ScrollBar { }
                delegate: MessageBubble {
                    required property string text
                    required property string role
                    required property string createdAt
                    required property string detailsText
                    width: messages.width
                    messageText: text
                    author: detailsText
                    outgoing: role === "User"
                    speechEnabled: true
                    onSpeakRequested: root.voice.speak(text)
                    timeText: createdAt
                }
            }
            AppButton {
                text: "Загрузить ещё"
                tone: "secondary"
                visible: root.client.messageHasMore
                enabled: root.client.serverActionsEnabled
                onClicked: root.client.loadMoreMessages()
            }
            AppTextArea {
                id: composer
                objectName: "chatComposer"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Theme.controlHeight
                Layout.maximumHeight: Theme.multilineHeight
                placeholderText: "Сообщение или команда, например: добавь задачу купить продукты"
                text: root.voice.draft
                onTextChanged: root.voice.draft = text
            }
            AppTextField {
                id: search
                objectName: "chatSearchQuery"
                Layout.fillWidth: true
                placeholderText: "Поисковая фраза (необязательно)"
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    objectName: "voiceStartButton"
                    text: "Говорить"
                    tone: "secondary"
                    enabled: root.voice.canStart
                    visible: !root.voice.canCancel
                    onClicked: root.voice.start()
                }
                AppButton {
                    objectName: "voiceFinishButton"
                    text: "Завершить запись"
                    tone: "secondary"
                    visible: root.voice.canFinish
                    onClicked: root.voice.finish()
                }
                AppButton {
                    objectName: "voiceCancelButton"
                    text: "Отменить запись"
                    tone: "secondary"
                    visible: root.voice.canCancel
                    onClicked: root.voice.cancel()
                }
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.voice.statusText
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    color: Theme.secondaryText
                }
                AppButton {
                    objectName: "stopSpeechButton"
                    text: "Остановить речь"
                    tone: "secondary"
                    visible: root.voice.speaking
                    onClicked: root.voice.stopSpeech()
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.voice.errorText
                textFormat: Text.PlainText
                visible: text.length > 0
                elide: Text.ElideRight
                maximumLineCount: 1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                color: Theme.danger
                HoverHandler { id: voiceErrorHover }
                Controls.ToolTip {
                    id: voiceErrorTip
                    visible: voiceErrorHover.hovered
                    text: root.voice.errorText
                    width: Theme.formWidth
                    margins: Theme.spaceSmall
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    palette.toolTipBase: Theme.raised
                    palette.toolTipText: Theme.text
                    contentItem: Text {
                        text: voiceErrorTip.text
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: Theme.text
                        font: voiceErrorTip.font
                    }
                }
            }
            RowLayout {
                AppButton {
                    objectName: "sendMessageButton"
                    text: "Отправить"
                    enabled: root.client.serverActionsEnabled && root.client.selectedChatIdentifier.length > 0
                    onClicked: root.client.sendMessage(composer.text, search.text)
                }
                AppButton {
                    text: "Повторить ответ"
                    tone: "secondary"
                    visible: root.client.errorEffect === "user_saved"
                    enabled: root.client.serverActionsEnabled
                    onClicked: root.client.retryMessage(root.client.savedUserIdentifier, search.text)
                }
            }
        }
    }
}
