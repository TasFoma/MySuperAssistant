import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts
import Msa.Ui.Common 1.0

Flickable {
    id: root
    required property var client
    required property var mobile
    objectName: "mobileChatScroll"
    clip: true
    contentWidth: width
    contentHeight: Math.max(height, Theme.mobileChatMinimumHeight)
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    Controls.ScrollBar.vertical: Controls.ScrollBar {}
    ColumnLayout {
        width: root.width
        height: root.contentHeight
        visible: root.mobile.conversationsVisible
        spacing: Theme.spaceSmall
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                Layout.fillWidth: true
                text: "Новая беседа"
                enabled: root.client.serverActionsEnabled
                onClicked: root.client.createChat()
            }
        }
        EmptyState {
            Layout.fillWidth: true
            visible: root.client.chats.count === 0
            title: "Нет загруженных бесед"
        }
        ListView {
            id: conversations
            objectName: "mobileConversationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spaceSmall
            model: root.client.chats
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
            delegate: AppButton {
                required property string identifier
                required property string title
                width: conversations.width
                text: title
                tone: root.client.selectedChatIdentifier === identifier ? "primary" : "secondary"
                onClicked: root.mobile.selectConversation(identifier)
            }
        }
    }
    ColumnLayout {
        width: root.width
        height: root.contentHeight
        visible: !root.mobile.conversationsVisible
        spacing: Theme.spaceSmall
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                objectName: "mobileConversationsButton"
                Layout.fillWidth: true
                text: "Беседы"
                tone: "secondary"
                onClicked: root.mobile.showConversations()
            }
            AppButton {
                text: search.visible ? "Скрыть поиск" : "Поиск"
                tone: "secondary"
                onClicked: search.visible = !search.visible
            }
            AppButton {
                text: "Ранее"
                visible: root.client.messageHasMore && root.client.selectedChatIdentifier.length > 0
                enabled: root.client.serverActionsEnabled
                onClicked: root.client.loadMoreMessages()
            }
        }
        SectionHeader {
            Layout.fillWidth: true
            visible: root.client.selectedChatIdentifier.length === 0
            description: "Откройте «Беседы» и выберите чат. Загруженные сообщения доступны без связи."
        }
        ListView {
            id: messages
            objectName: "mobileMessageList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.controlHeight
            clip: true
            spacing: Theme.spaceSmall
            model: root.client.messages
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
            delegate: MessageBubble {
                required property string text
                required property string role
                required property string detailsText
                required property string createdAt
                width: messages.width
                messageText: text
                author: detailsText
                outgoing: role === "User"
                timeText: createdAt
            }
        }
        AppTextArea {
            id: composer
            objectName: "mobileComposer"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Theme.controlHeight
            Layout.maximumHeight: Theme.multilineHeight
            text: root.mobile.draft
            placeholderText: "Сообщение или команда"
            onTextChanged: root.mobile.draft = text
        }
        AppTextField {
            id: search
            objectName: "mobileSearchQuery"
            Layout.fillWidth: true
            visible: false
            text: root.mobile.searchQuery
            placeholderText: "Поисковая фраза (необязательно)"
            onTextEdited: root.mobile.searchQuery = text
        }
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                objectName: "mobileSendButton"
                Layout.fillWidth: true
                text: "Отправить"
                enabled: root.client.serverActionsEnabled && root.client.selectedChatIdentifier.length > 0
                onClicked: root.mobile.sendMessage()
            }
            AppButton {
                text: "Повторить"
                tone: "secondary"
                visible: root.client.errorEffect === "user_saved"
                enabled: root.client.serverActionsEnabled
                onClicked: root.mobile.retryMessage()
            }
        }
    }
}
