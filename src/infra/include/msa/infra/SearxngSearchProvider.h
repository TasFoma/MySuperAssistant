#pragma once

#include "msa/core/WebSearchProvider.h"
#include "msa/infra/HttpGetClient.h"

namespace msa::infra
{

/// @brief Один GET к JSON API SearXNG без чтения найденных страниц и повторов.
class SearxngSearchProvider final : public core::WebSearchProvider
{
public:
    /// @brief Полный endpoint /search и настройки задаёт приложение.
    /// Нужен JSON-формат в настройках SearXNG; неверный endpoint — invalid_argument.
    SearxngSearchProvider(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings);

protected:
    /// @brief Передаёт только query; историю, профиль и исходный диалог не получает.
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::WebSearchResult>>
    requestSearch(const std::string& query) override;

private:
    HttpGetClient m_http;
    QUrl m_endpoint{};
};

} // namespace msa::infra
