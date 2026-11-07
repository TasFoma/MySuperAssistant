#pragma once

#include "msa/core/ChatService.h"
#include "msa/core/Clock.h"
#include "msa/core/DailyContextService.h"
#include "msa/core/EveningCheckInRepository.h"
#include "msa/core/EveningCheckInService.h"
#include "msa/core/LlmClient.h"
#include "msa/core/MoodService.h"
#include "msa/core/TransactionRunner.h"
#include "msa/core/UserProfileService.h"
#include "msa/core/WebSearchProvider.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Успешно сохранённый ответ и необязательная запись дневника.
struct ConversationReply {
    /// @brief ID сохранённого сообщения Assistant.
    std::int64_t messageIdentifier{0};
    /// @brief Полный текст с подтверждением только после успешной записи настроения.
    std::string text{};
    /// @brief Тон для текущей реакции, сам не является записью дневника.
    std::optional<MoodScore> tone{};
    /// @brief ID созданной записи дневника, если была явная запись.
    std::optional<std::int64_t> moodIdentifier{};
    /// @brief ID новых фактов/версий, сохранённых из текущего сообщения.
    std::vector<std::int64_t> profileFactIdentifiers{};
    /// @brief Реальные материалы явного поиска; список также добавляется в текст программой.
    std::vector<WebSearchResult> searchSources{};
};

/// @brief Отказ из-за ввода, бюджета или отсутствия сообщения для повтора.
struct ConversationRejection {
    /// @brief Понятная причина для вызывающего кода.
    std::string reason{};
};

/// @brief Сохранённый ответ, предметный отказ либо ошибка LLM/поиска без записи ответа.
using ConversationResult = std::variant<ConversationReply, ConversationRejection, LlmFailure, ExternalServiceError>;

/// @brief Связывает контекст, чат, ментора и явный дневник без HTTP и Qt.
class ConversationService {
public:
    /// @brief Все зависимости невладеющие, часы и репозиторий опроса общие с его сервисом.
    /// Репозитории чата, дневника, профиля и опроса используют общий TransactionRunner.
    ConversationService(const Clock& clock, ChatService& chat, UserProfileService& profile,
        const DailyContextService& daily, LlmClient& client, MoodService& moods,
        EveningCheckInService& checkIn, EveningCheckInRepository& checkIns, TransactionRunner& transactions,
        WebSearchProvider& search);
    /// @brief Публикует опрос в текущем чате и сохраняет ожидание; nullopt — опрос не нужен.
    /// Очистка старого ожидания, дата, сообщение и новое ожидание — одна транзакция.
    /// ID возвращается только после успешной фиксации; при исключении изменения откатываются.
    /// Истёкшее ожидание очищается при следующей операции. Доставка клиентам — вне сервиса.
    [[nodiscard]] std::optional<std::int64_t> offerEveningCheckIn();
    /// @brief Сохраняет User целиком и делает одну попытку ответа по явному бюджету текста.
    /// При отказе контекста или LLM сообщение User остаётся в истории для явного повтора.
    [[nodiscard]] ConversationResult sendMessage(const std::string& message, std::size_t maxChars);
    /// @brief Явно ищет только query, затем отвечает с материалами; без скрытого ответа при сбое поиска.
    /// Непустой query задаёт вызывающий код по действию пользователя. История/профиль в поиск не передаются.
    [[nodiscard]] ConversationResult sendMessageWithSearch(
        const std::string& message, const std::string& query, std::size_t maxChars);
    /// @brief Повторяет обработку последнего User без повторной записи его текста.
    /// После ошибки записи повтор допустим при подтверждённом откате исправным хранилищем.
    /// При отказе отката сначала требуется восстановить соединение и перечитать историю.
    /// При последнем Assistant отказывает. Успешная обработка закрывает опрос в этом чате.
    [[nodiscard]] ConversationResult retryLastMessage(std::size_t maxChars);
    /// @brief Явный повтор поиска и обработки последнего User без его дублирования.
    /// query передаётся заново; режим поиска и фраза отдельно в истории не сохраняются.
    /// Обычный retryLastMessage явно выполняет обработку без веб-поиска.
    [[nodiscard]] ConversationResult retryLastMessageWithSearch(const std::string& query, std::size_t maxChars);

private:
    [[nodiscard]] std::optional<PendingEveningCheckIn> currentPendingCheckIn();
    [[nodiscard]] ConversationResult processLastMessage(std::size_t maxChars, const std::optional<std::string>& searchQuery);

    const Clock& m_clock;
    ChatService& m_chat;
    UserProfileService& m_profile;
    const DailyContextService& m_daily;
    LlmClient& m_client;
    MoodService& m_moods;
    EveningCheckInService& m_checkIn;
    EveningCheckInRepository& m_checkIns;
    TransactionRunner& m_transactions;
    WebSearchProvider& m_search;
};

}  // namespace msa::core

