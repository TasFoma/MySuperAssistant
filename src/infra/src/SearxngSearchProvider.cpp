#include "msa/infra/SearxngSearchProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

[[nodiscard]] core::ExternalServiceError invalidResponse()
{
    return {core::ExternalServiceErrorKind::InvalidResponse, "SearXNG вернул неверный ответ поиска."};
}

} // namespace

SearxngSearchProvider::SearxngSearchProvider(QNetworkAccessManager& manager, QUrl endpoint, HttpGetSettings settings) :
    m_http{manager, std::move(settings)}, m_endpoint{std::move(endpoint)}
{
    if (!HttpGetClient::validEndpoint(m_endpoint))
    {
        throw std::invalid_argument{"Некорректный адрес SearXNG."};
    }
}

core::ExternalServiceResult<std::vector<core::WebSearchResult>>
SearxngSearchProvider::requestSearch(const std::string& query)
{
    constexpr std::size_t kMaximumSources = 3;
    auto url = m_endpoint;
    QUrlQuery parameters;
    // QUrlQuery ждёт percent-encoded ввод; буквальный '+' иначе станет пробелом на сервере.
    parameters.addQueryItem("q", QString::fromLatin1(QUrl::toPercentEncoding(QString::fromStdString(query))));
    parameters.addQueryItem("format", "json");
    url.setQuery(parameters);
    const auto result = m_http.get(url);
    if (const auto* error = std::get_if<core::ExternalServiceError>(&result))
    {
        return *error;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(std::get<QByteArray>(result), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return invalidResponse();
    }
    const auto root = document.object();
    if (root.contains("error") || !root["results"].isArray())
    {
        return invalidResponse();
    }
    std::vector<core::WebSearchResult> sources;
    for (const auto& value : root["results"].toArray())
    {
        if (!value.isObject())
        {
            return invalidResponse();
        }
        const auto object = value.toObject();
        if ((object.contains("title") && !object["title"].isString()) ||
            (object.contains("url") && !object["url"].isString() && !object["url"].isNull()) ||
            (object.contains("content") && !object["content"].isString()))
        {
            return invalidResponse();
        }
        const auto title = object["title"].toString();
        const auto address = object["url"].toString();
        const QUrl sourceUrl{address, QUrl::StrictMode};
        if (title.trimmed().isEmpty() || !sourceUrl.isValid() || sourceUrl.host().isEmpty() ||
            !core::validWebSourceUrl(address.toStdString()))
        {
            continue;
        }
        sources.push_back({title.toStdString(), address.toStdString(), object["content"].toString().toStdString()});
        if (sources.size() == kMaximumSources)
        {
            break;
        }
    }

    return sources;
}

} // namespace msa::infra
