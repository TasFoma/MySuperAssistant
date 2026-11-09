import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Item {
    id: root
    required property var client
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
                Layout.maximumHeight: Theme.multilineHeight
                enabled: root.client.selectedChatIdentifier.length > 0
                placeholderText: "Сообщение или команда, например: добавь задачу купить продукты"
            }
            AppTextField {
                id: search
                objectName: "chatSearchQuery"
                Layout.fillWidth: true
                placeholderText: "Поисковая фраза (необязательно)"
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
