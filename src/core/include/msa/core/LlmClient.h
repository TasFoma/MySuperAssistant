#pragma once

#include "msa/core/LlmMessage.h"
#include "msa/core/MoodEntry.h"
#include "msa/core/ProfileFactProposal.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Готовый запрос; один вызов возвращает полный ответ без потоковой выдачи.
struct LlmRequest {
    /// @brief System, история User/Assistant, затем полный текущий вопрос User.
    std::vector<LlmMessage> messages{};
    /// @brief Вызывающий код ожидает ответ на вечерний опрос в текущем чате.
    bool awaitingEveningCheckIn{false};
    /// @brief ID актуальных фактов, реально вошедших в контекст; только их можно исправлять.
    std::vector<std::int64_t> profileFactIdentifiers{};
};

/// @brief Распознанная моделью причина явной записи настроения.
enum class MoodRecordingReason : std::uint8_t {
    ExplicitRequest,
    EveningCheckInReply,
};

/// @brief Предложение записи, которое обрабатывает вызывающий код через MoodService.
struct MoodRecordingProposal {
    /// @brief Оценка для дневника; независима от текущего тона переписки.
    MoodScore score{MoodScore::Neutral};
    /// @brief Явная просьба либо ответ на ожидаемый вечерний опрос.
    MoodRecordingReason reason{MoodRecordingReason::ExplicitRequest};
    /// @brief Полный исходный текущий вопрос; complete заполняет его независимо от модели.
    std::string note{};
};

/// @brief Проверенный полный ответ; сам по себе не меняет историю или дневник.
struct LlmResponse {
    /// @brief Непустой текст для пользователя без служебного JSON, сохраняется полностью.
    std::string reply{};
    /// @brief Оценка тона для текущей реакции; nullopt означает «не определён».
    std::optional<MoodScore> tone{};
    /// @brief Только явная запись; отсутствие предложения не создаёт запись по тону.
    /// Заметку заполняет complete из request.messages.back().text целиком.
    std::optional<MoodRecordingProposal> moodRecording{};
    /// @brief Факты текущего User для автоматического сохранения без назначения тегов.
    std::vector<ProfileFactProposal> profileFacts{};
};

/// @brief Вид ожидаемой ошибки запроса, формата ответа или внешнего сервиса.
enum class LlmFailureKind : std::uint8_t {
    InvalidRequest,
    InvalidResponse,
    Unavailable,
};

/// @brief Ошибка без частичного ответа и действий над хранилищем.
struct LlmFailure {
    /// @brief Категория ошибки для вызывающего кода.
    LlmFailureKind kind{LlmFailureKind::Unavailable};
    /// @brief Понятная причина без токенов доступа и тела технического ответа сервера.
    std::string reason{};
};

/// @brief Полный проверенный ответ либо ошибка; обе альтернативы не содержат Qt-типов.
using LlmResult = std::variant<LlmResponse, LlmFailure>;

/// @brief Общие проверки и контракт реализации LLM без файлов, сети и хранилища.
class LlmClient {
public:
    /// @brief Освобождает реализацию клиента.
    virtual ~LlmClient();

    /// @brief Проверяет запрос, делает одну попытку и проверяет полный результат.
    /// Запрос содержит непустые тексты, один System в начале, текущий User в конце;
    /// промежуточные роли — User/Assistant. Неверный ввод не вызывает реализацию.
    /// Неизвестные оценки/причины, пустой ответ и ответ на неожидаемый опрос — отказ.
    /// Нет повторных запросов или записи данных. Вызов синхронный: будущий сетевой
    /// клиент нельзя вызывать в потоке UI. Бюджет проверяется до вызова сборщиком.
    [[nodiscard]] LlmResult complete(const LlmRequest& request);

protected:
    /// @brief Одна попытка получения полного ответа; вызывается после проверки запроса.
    /// Реализация infra проверяет JSON и преобразует ошибки транспорта в LlmFailure.
    /// Ошибки выделения памяти и дефекты реализации могут передаваться исключениями.
    [[nodiscard]] virtual LlmResult requestCompletion(const LlmRequest& request) = 0;
};

}  // namespace msa::core

