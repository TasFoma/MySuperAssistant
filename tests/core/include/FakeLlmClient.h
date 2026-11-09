#pragma once

#include "msa/core/LlmClient.h"

#include <vector>

namespace msa::core {

/// @brief Клиент без сети с заданным результатом и снимками полученных запросов.
class FakeLlmClient final : public LlmClient {
public:
    /// @brief Задаёт ответ либо ошибку одной попытки.
    explicit FakeLlmClient(LlmResult result);
    /// @brief Возвращает снимки запросов; неверный ввод в них не попадает.
    [[nodiscard]] const std::vector<LlmRequest>& requests() const;
    /// @brief Задаёт результат последующей явной попытки, не очищая историю запросов.
    void setResult(LlmResult result);

protected:
    /// @brief Сохраняет копию запроса и возвращает настроенный результат без повторов.
    [[nodiscard]] LlmResult requestCompletion(const LlmRequest& request) override;

private:
    LlmResult m_result;
    std::vector<LlmRequest> m_requests{};
};

}  // namespace msa::core

