#pragma once

#include "ServerSettings.h"

#include "msa/infra/LlamaClient.h"
#include "msa/infra/NominatimGeocoder.h"
#include "msa/infra/OpenMeteoWeatherProvider.h"
#include "msa/infra/OsrmRoutePlanner.h"
#include "msa/infra/SearxngSearchProvider.h"

#include <QEventLoop>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QThread>

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace msa::server {

/// @brief Сетевые объекты создаются, используются и уничтожаются только рабочим потоком.
struct NetworkBackend {
    explicit NetworkBackend(const ServerSettings& settings);
    QNetworkAccessManager manager{};
    infra::LlamaClient llama;
    infra::NominatimGeocoder geocoder;
    infra::OsrmRoutePlanner planner;
    infra::OpenMeteoWeatherProvider weather;
    infra::SearxngSearchProvider search;
};

/// @brief Исполнитель HTTP без доступа к SQLite. Вызывается последовательно владельцем сервера.
/// Ожидание результата обслуживает события владельца; диспетчер запрещает повторные записи.
class NetworkServices final : public core::LlmClient, public core::Geocoder, public core::RoutePlanner,
    public core::WeatherProvider, public core::WebSearchProvider {
public:
    /// @brief Создаёт рабочий поток и постоянные адаптеры, сохраняя ограничитель Nominatim.
    explicit NetworkServices(const ServerSettings& settings);
    /// @brief Дожидается завершения работы и уничтожает адаптеры в их потоке.
    ~NetworkServices() override;
    /// @brief Ключ постоянного кэша с endpoint и параметрами Nominatim.
    [[nodiscard]] std::string geocodingSourceKey();

protected:
    [[nodiscard]] core::LlmResult requestCompletion(const core::LlmRequest& request) override;
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::GeocodedPlace>>
        requestPlaces(const std::string& place) override;
    [[nodiscard]] core::ExternalServiceResult<core::RoutePlan> requestRoute(
        core::GeoCoordinates origin, core::GeoCoordinates destination, core::TravelMode mode) override;
    [[nodiscard]] core::ExternalServiceResult<core::WeatherForecast>
        requestForecast(core::GeoCoordinates coordinates) override;
    [[nodiscard]] core::ExternalServiceResult<std::vector<core::WebSearchResult>>
        requestSearch(const std::string& query) override;

private:
    template <typename Operation> auto run(Operation operation) -> decltype(operation())
    {
        if (m_running) { throw std::logic_error{"Повторный вход в сетевой исполнитель"}; }
        m_running = true;
        using Result = decltype(operation());
        std::optional<Result> result;
        std::exception_ptr failure;
        bool done{false};
        QEventLoop loop;
        const auto queued = QMetaObject::invokeMethod(&m_context,
            [operation, &result, &failure, &loop, &done]() {
                try { result.emplace(operation()); }
                catch (...) { failure = std::current_exception(); }
                QMetaObject::invokeMethod(&loop, [&loop, &done]() { done = true; loop.quit(); }, Qt::QueuedConnection);
            }, Qt::QueuedConnection);
        if (!queued) {
            m_running = false;
            throw std::runtime_error{"Не удалось запустить сетевой запрос"};
        }
        // Только queued-завершение читает результат после записи рабочим потоком.
        while (!done) { loop.exec(); }
        m_running = false;
        if (failure) { std::rethrow_exception(failure); }

        return std::move(*result);
    }

    QThread m_worker{};
    QObject m_context{};
    std::unique_ptr<NetworkBackend> m_backend{};
    bool m_running{false};
};
} // namespace msa::server

