#pragma once

#include "msa/core/ExternalServiceResult.h"

#include <string>
#include <vector>

namespace msa::core
{

/// @brief Внешний материал поиска, не сообщение пользователя и не факт профиля.
struct WebSearchResult
{
    /// @brief Название страницы.
    std::string title{};
    /// @brief Абсолютный HTTP(S)-адрес без учётных данных и управляющих символов.
    std::string url{};
    /// @brief Короткое описание, не полный текст страницы.
    std::string snippet{};
};

/// @brief Минимальная проверка адреса источника в core; HTTP-адаптер проверяет полный URL.
[[nodiscard]] bool validWebSourceUrl(const std::string& url);
/// @brief Данные для модели с явной меткой недоверенного внешнего содержимого.
[[nodiscard]] std::string webSearchContext(const std::vector<WebSearchResult>& sources);
/// @brief Список переданных модели материалов для сохранения вместе с ответом.
[[nodiscard]] std::string webSearchSourcesText(const std::vector<WebSearchResult>& sources);

/// @brief Явный веб-поиск без доступа к истории чата и профилю пользователя.
class WebSearchProvider
{
public:
    /// @brief Освобождает реализацию.
    virtual ~WebSearchProvider();
    /// @brief Проверяет запрос и одну полную выдачу; возвращает до трёх источников.
    /// Описания ограничены 300 кодовыми точками Unicode. Пустая выдача — NotFound,
    /// неверные данные — InvalidResponse, неверный запрос не вызывает реализацию.
    [[nodiscard]] ExternalServiceResult<std::vector<WebSearchResult>> search(const std::string& query);

protected:
    /// @brief Одна попытка поиска только по явно переданной фразе.
    [[nodiscard]] virtual ExternalServiceResult<std::vector<WebSearchResult>>
    requestSearch(const std::string& query) = 0;
};

} // namespace msa::core
