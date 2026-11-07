#pragma once

#include "msa/core/LlmClient.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>

#include <chrono>

namespace msa::infra {

/// @brief Настройки одного llama-server; личные значения задаёт приложение.
struct LlamaClientSettings {
    /// @brief Полный HTTP(S)-адрес /v1/chat/completions без пароля и query.
    QUrl completionUrl{};
    /// @brief Имя модели; пустое значение оставляет выбор серверу.
    QString model{};
    /// @brief Необязательный Bearer-токен; не включается в сообщения об ошибках.
    QByteArray apiKey{};
    /// @brief Общий срок одного запроса, включая получение тела; обязателен.
    std::chrono::milliseconds timeout{};
    /// @brief Положительный лимит генерации; не является проверкой окна контекста.
    int maxResponseTokens{};
};

/// @brief Синхронный HTTP-адаптер полного JSON-ответа без повторов и хранилищ.
/// Нужен QCoreApplication. Менеджер, клиент и вызовы должны находиться в одном
/// рабочем потоке с диспетчером событий. Вложенный цикл обрабатывает события этого
/// потока: приложения не должны повторно входить в сервисы во время запроса.
class LlamaClient final : public core::LlmClient {
public:
    /// @brief Проверяет настройки; неверные значения вызывают invalid_argument.
    /// @param manager Внешний менеджер, который должен пережить клиент и его вызовы.
    /// @param settings Адрес, авторизация, срок и лимит генерации.
    LlamaClient(QNetworkAccessManager& manager, LlamaClientSettings settings);

protected:
    /// @brief Выполняет один POST, проверяет конверт и строгий JSON модели.
    [[nodiscard]] core::LlmResult requestCompletion(const core::LlmRequest& request) override;

private:
    QNetworkAccessManager& m_manager;
    LlamaClientSettings m_settings{};
    bool m_inProgress{false};
};

}  // namespace msa::infra

