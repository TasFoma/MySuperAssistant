#pragma once

#include "ServerSettings.h"

#include "msa/core/CachedGeocoder.h"
#include "msa/core/CommandParser.h"
#include "msa/core/ConversationService.h"
#include "msa/core/EventService.h"
#include "msa/core/EventWarningService.h"
#include "msa/core/GreetingService.h"
#include "msa/core/NavigationService.h"
#include "msa/core/PlaceReminderLocationService.h"
#include "msa/core/ReminderService.h"
#include "msa/infra/SqliteChatRepository.h"
#include "msa/infra/SqliteEveningCheckInRepository.h"
#include "msa/infra/SqliteEventRepository.h"
#include "msa/infra/SqliteEventWarningRepository.h"
#include "msa/infra/SqliteGeocodingCacheRepository.h"
#include "msa/infra/SqliteGreetingRepository.h"
#include "msa/infra/SqliteHabitRepository.h"
#include "msa/infra/SqliteMoodRepository.h"
#include "msa/infra/SqliteReminderRepository.h"
#include "msa/infra/SqliteRouteHistoryRepository.h"
#include "msa/infra/SqliteTaskRepository.h"
#include "msa/infra/SqliteUserProfileRepository.h"
#include "msa/infra/TcpServer.h"

#include <QJsonArray>
#include <QObject>
#include <QTimer>

#include <map>
#include <optional>
#include <set>

namespace msa::server {

/// @brief Ссылки на внешние исполнители: настоящие рабочие адаптеры либо управляемые тестовые.
struct ExternalServices {
    core::LlmClient& llama;
    core::Geocoder& geocoder;
    core::RoutePlanner& planner;
    core::WeatherProvider& weather;
    core::WebSearchProvider& search;
    std::string geocodingSourceKey;
};

/// @brief Связка сервера версии 1: все репозитории и предметные изменения принадлежат её потоку.
class ServerRuntime final : public QObject {
public:
    /// @brief Зависимости и часы должны пережить сервер; открывает БД и восстанавливает основной чат.
    ServerRuntime(const ServerSettings& settings, const core::Clock& clock, ExternalServices external);
    /// @brief Останавливает таймер и закрывает сессии до уничтожения зависимостей обработчиков.
    ~ServerRuntime() override;
    /// @brief Пропускает старые однократные напоминания, начинает слушать и проверять сроки.
    [[nodiscard]] bool start();
    /// @brief Фактический порт, в том числе выбранный ОС при настройке port=0.
    [[nodiscard]] quint16 port() const;

private:
    void receive(quint64 connectionId, const infra::ProtocolRequest& request);
    void dispatch(quint64 connectionId, const infra::ProtocolRequest& request);
    [[nodiscard]] QJsonObject execute(quint64 connectionId, const infra::ProtocolRequest& request,
        std::optional<QString>& savedUser);
    [[nodiscard]] QJsonObject read(const infra::ProtocolRequest& request);
    [[nodiscard]] QJsonObject conversation(const infra::ProtocolRequest& request, std::optional<QString>& savedUser);
    [[nodiscard]] QJsonObject command(const infra::ProtocolRequest& request);
    [[nodiscard]] QJsonObject route(const QJsonObject& params);
    [[nodiscard]] QJsonObject prepareLocation(quint64 connectionId, const QJsonObject& params);
    void checkDeadlines();
    void closeSession(quint64 connectionId);
    void changed(const QJsonArray& scopes);
    void chatChanged(std::int64_t chatIdentifier);
    void assistantMessage(std::int64_t messageIdentifier);
    void reminderDue(const core::Reminder& reminder);

    ServerSettings m_settings;
    const core::Clock& m_clock;
    ExternalServices m_external;
    infra::SqliteDatabase m_database;
    infra::SqliteChatRepository m_chats;
    infra::SqliteTaskRepository m_tasks;
    infra::SqliteEventRepository m_events;
    infra::SqliteReminderRepository m_reminders;
    infra::SqliteHabitRepository m_habits;
    infra::SqliteMoodRepository m_moods;
    infra::SqliteUserProfileRepository m_profile;
    infra::SqliteGreetingRepository m_greetings;
    infra::SqliteEveningCheckInRepository m_checkIns;
    infra::SqliteEventWarningRepository m_warnings;
    infra::SqliteGeocodingCacheRepository m_cache;
    infra::SqliteRouteHistoryRepository m_routes;
    core::TaskService m_taskService;
    core::EventService m_eventService;
    core::ReminderService m_reminderService;
    core::HabitService m_habitService;
    core::MoodService m_moodService;
    core::UserProfileService m_profileService;
    core::CalendarService m_calendar;
    core::DailyContextService m_daily;
    core::GreetingService m_greetingService;
    core::EveningCheckInService m_checkInService;
    core::EventWarningService m_warningService;
    core::CachedGeocoder m_geocoder;
    core::RouteHistoryService m_routeHistory;
    core::NavigationService m_navigation;
    core::PlaceReminderLocationService m_locations;
    std::int64_t m_mainChat{};
    infra::TcpServer m_transport;
    QTimer m_timer{};
    // Предложения адресов принадлежат исходной TCP-сессии, а не только номеру предложения.
    std::map<std::int64_t, std::pair<quint64, std::int64_t>> m_proposals{};
    std::set<quint64> m_closedSessions{};
    bool m_busy{false};
    bool m_checking{false};
};
} // namespace msa::server

