#pragma once

#include "msa/core/WebSearchProvider.h"

#include <functional>

namespace msa::core
{

/// @brief Тестовый поиск с заданной выдачей и снимками только поисковых фраз.
class FakeWebSearchProvider final : public WebSearchProvider
{
public:
    /// @brief Заданная выдача или ошибка; по умолчанию поиск ничего не находит.
    ExternalServiceResult<std::vector<WebSearchResult>> result{std::vector<WebSearchResult>{}};
    /// @brief Фразы, реально переданные в реализацию после проверки ввода.
    std::vector<std::string> queries{};
    /// @brief Дополнительное действие во время поиска, например перевод часов.
    std::function<void()> onSearch{};

protected:
    /// @brief Сохраняет фразу и возвращает копию результата.
    [[nodiscard]] ExternalServiceResult<std::vector<WebSearchResult>> requestSearch(const std::string& query) override;
};

} // namespace msa::core
