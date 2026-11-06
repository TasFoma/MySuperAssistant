#pragma once

#include <string>

namespace msa::core {

/// @brief Согласованные инструкции ментора и формат полного ответа модели.
class MentorPrompt {
public:
    /// @brief Возвращает инструкции с явным состоянием ожидания вечернего ответа.
    /// Формат JSON соответствует LlmClient; исходная заметка моделью не формируется.
    [[nodiscard]] std::string build(bool awaitingEveningCheckIn) const;
};

}  // namespace msa::core

