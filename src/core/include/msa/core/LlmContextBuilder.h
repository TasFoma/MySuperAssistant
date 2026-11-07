#pragma once

#include "msa/core/ChatMessage.h"
#include "msa/core/LlmMessage.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Снимок данных одного чата для сборки запроса; сборщик не меняет входные данные.
struct LlmContextInput {
    /// @brief Обязательные инструкции модели; текст задаёт вызывающая сторона.
    std::string systemPrompt{};
    /// @brief Обязательный текущий вопрос, включаемый полностью в конец запроса.
    std::string currentQuestion{};
    /// @brief Уже выбранные факты профиля в порядке приоритета; тематического поиска здесь нет.
    std::vector<std::string> selectedFacts{};
    /// @brief Готовый текст DailyContextService; пустая строка означает отсутствие сводки.
    std::string dailyContext{};
    /// @brief Сохранённые сообщения одного чата; сборщик упорядочивает их по ID.
    std::vector<ChatMessage> history{};
    /// @brief Если вопрос уже сохранён, его ID исключает его и более поздние записи из истории.
    /// При отсутствии ID история должна содержать только сообщения до текущего вопроса.
    std::optional<std::int64_t> currentMessageId{};
    /// @brief Необязательные внешние данные, отдельным User перед вопросом, целиком в бюджете.
    /// Это не пользовательская биография и не инструкции System.
    std::string externalContext{};
};

/// @brief Подготовленный запрос и сведения о частях, не вошедших в бюджет.
struct LlmContext {
    /// @brief Инструкции с данными, история и текущий вопрос в порядке отправки.
    std::vector<LlmMessage> messages{};
    /// @brief Число кодовых точек во всех текстах, включая заголовки и разделители.
    std::size_t usedChars{0};
    /// @brief Число выбранных фактов, не вошедших в запрос.
    std::size_t omittedFacts{0};
    /// @brief Включена ли непустая сводка дня целиком.
    bool dailyContextIncluded{false};
    /// @brief Число предшествующих сообщений, не вошедших в запрос.
    std::size_t omittedHistoryMessages{0};
};

/// @brief Причина отказа из-за неверного ввода или недостаточного общего бюджета.
struct LlmContextRejection {
    /// @brief Объяснение для вызывающей стороны.
    std::string reason;
};

/// @brief Готовый контекст либо причина отказа.
using LlmContextResult = std::variant<LlmContext, LlmContextRejection>;

/// @brief Собирает контекст без хранилища, сети, токенизации и обращения к модели.
class LlmContextBuilder {
public:
    /// @brief Резервирует инструкции, вопрос и явные внешние данные, затем факты, сводку и историю.
    /// Факты — целый префикс по приоритету, сводка — целиком, история — непрерывный суффикс.
    /// Бюджет явный и учитывает кодовые точки текстов, не токены модели.
    /// Пустые инструкции/вопрос, неверные данные истории или нехватка обязательного бюджета — отказ.
    [[nodiscard]] LlmContextResult build(const LlmContextInput& input, std::size_t maxChars) const;
};

}  // namespace msa::core

