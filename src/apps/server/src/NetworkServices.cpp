#include "msa/server/NetworkServices.h"

namespace msa::server {

NetworkBackend::NetworkBackend(const ServerSettings& settings)
    : llama{manager, settings.llama}
{
    if (!settings.servicesEnabled) { return; }
    geocoder.emplace(manager, settings.nominatim, settings.http);
    planner.emplace(manager, settings.osrm, settings.http);
    weather.emplace(manager, settings.weather, settings.http);
    search.emplace(manager, settings.search, settings.http);
}

NetworkServices::NetworkServices(const ServerSettings& settings)
{
    m_context.moveToThread(&m_worker);
    m_worker.start();
    try {
        static_cast<void>(run([this, settings]() {
            m_backend = std::make_unique<NetworkBackend>(settings);
            return true;
        }));
    } catch (...) {
        auto* owner = QThread::currentThread();
        QMetaObject::invokeMethod(&m_context, [this, owner]() { m_context.moveToThread(owner); },
            Qt::BlockingQueuedConnection);
        m_worker.quit();
        m_worker.wait();
        throw;
    }
}

NetworkServices::~NetworkServices()
{
    auto* owner = QThread::currentThread();
    QMetaObject::invokeMethod(&m_context, [this, owner]() {
        m_backend.reset();
        m_context.moveToThread(owner);
    }, Qt::BlockingQueuedConnection);
    m_worker.quit();
    m_worker.wait();
}

std::string NetworkServices::geocodingSourceKey()
{
    return run([this]() {
        return m_backend->geocoder ? m_backend->geocoder->cacheSourceKey() : std::string{"services-disabled"};
    });
}

core::LlmResult NetworkServices::requestCompletion(const core::LlmRequest& request)
{
    return run([this, request]() { return m_backend->llama.complete(request); });
}

core::ExternalServiceResult<std::vector<core::GeocodedPlace>> NetworkServices::requestPlaces(const std::string& place)
{
    return run([this, place]() -> core::ExternalServiceResult<std::vector<core::GeocodedPlace>> {
        if (!m_backend->geocoder) {
            return core::ExternalServiceError{core::ExternalServiceErrorKind::Unavailable,
                "Поиск мест отключён в настройках сервера."};
        }

        return m_backend->geocoder->search(place);
    });
}

core::ExternalServiceResult<core::RoutePlan> NetworkServices::requestRoute(
    core::GeoCoordinates origin, core::GeoCoordinates destination, core::TravelMode mode)
{
    return run([this, origin, destination, mode]() -> core::ExternalServiceResult<core::RoutePlan> {
        if (!m_backend->planner) {
            return core::ExternalServiceError{core::ExternalServiceErrorKind::Unavailable,
                "Маршруты отключены в настройках сервера."};
        }

        return m_backend->planner->route(origin, destination, mode);
    });
}

core::ExternalServiceResult<core::WeatherForecast> NetworkServices::requestForecast(core::GeoCoordinates coordinates)
{
    return run([this, coordinates]() -> core::ExternalServiceResult<core::WeatherForecast> {
        if (!m_backend->weather) {
            return core::ExternalServiceError{core::ExternalServiceErrorKind::Unavailable,
                "Погода отключена в настройках сервера."};
        }

        return m_backend->weather->forecast(coordinates);
    });
}

core::ExternalServiceResult<std::vector<core::WebSearchResult>> NetworkServices::requestSearch(const std::string& query)
{
    return run([this, query]() -> core::ExternalServiceResult<std::vector<core::WebSearchResult>> {
        if (!m_backend->search) {
            return core::ExternalServiceError{core::ExternalServiceErrorKind::Unavailable,
                "Веб-поиск отключён в настройках сервера."};
        }

        return m_backend->search->search(query);
    });
}
} // namespace msa::server

