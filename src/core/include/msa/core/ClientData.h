#pragma once

#include "msa/core/Chat.h"
#include "msa/core/ChatMessage.h"
#include "msa/core/ClientPlannerAction.h"
#include "msa/core/Event.h"
#include "msa/core/HabitService.h"
#include "msa/core/MoodEntry.h"
#include "msa/core/Reminder.h"
#include "msa/core/RouteHistoryService.h"
#include "msa/core/Task.h"
#include "msa/core/UserProfile.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core
{

/// @brief Состояние клиентского представления без зависимости от Qt и транспорта.
enum class ClientConnectionState
{
    Disconnected,
    Connecting,
    Authenticating,
    Synchronizing,
    Ready,
    WaitingToReconnect,
    Exhausted
};
/// @brief Вхождение серии на конкретную дату, исходная дата события сохраняется.
struct ClientEventOccurrence
{
    Event event;
    std::chrono::year_month_day occurrenceDate;
};
/// @brief Целый начальный снимок, принимаемый представлением только после полного разбора.
struct ClientSnapshot
{
    std::chrono::local_seconds serverTime{};
    std::string timeZone{};
    std::int64_t mainChatIdentifier{};
    std::vector<Task> tasks{};
    std::vector<Reminder> reminders{};
    std::vector<HabitProgress> habits{};
    std::vector<ClientEventOccurrence> eventsToday{};
};
/// @brief Чтения, которые нужны общим моделям клиента.
enum class ClientReadKind
{
    Snapshot,
    Chats,
    Messages,
    CalendarDay,
    CalendarMonth,
    Profile,
    Moods,
    Routes
};
/// @brief Адрес типизированного чтения; используются только поля выбранного kind.
struct ClientReadRequest
{
    ClientReadKind kind{ClientReadKind::Snapshot};
    std::int64_t chatIdentifier{};
    std::optional<std::int64_t> afterMessageIdentifier{};
    int limit{100};
    std::chrono::year_month_day date{};
    std::chrono::year_month month{};
    int days{14};
};
/// @brief Общая библиотека бесед без общего выбранного экрана.
struct ClientChatList
{
    std::vector<Chat> chats{};
    std::int64_t mainChatIdentifier{};
};
/// @brief Страница истории с адресом исходного запроса и проверенным продвижением курсора.
struct ClientMessagePage
{
    std::int64_t chatIdentifier{};
    std::optional<std::int64_t> afterMessageIdentifier{};
    std::vector<ChatMessage> messages{};
    std::optional<std::int64_t> nextAfterMessageIdentifier{};
    bool hasMore{false};
};
/// @brief Вхождения одного выбранного дня.
struct ClientCalendarDay
{
    std::chrono::year_month_day date;
    std::vector<ClientEventOccurrence> events;
};
/// @brief Упорядоченные дни месяца, содержащие события.
struct ClientCalendarMonth
{
    std::chrono::year_month month;
    std::vector<unsigned> days;
};
/// @brief Все версии фактов профиля, включая прежние источники.
struct ClientProfile
{
    std::vector<UserProfileFact> facts;
};
/// @brief Полные записи дневника выбранного периода.
struct ClientMoods
{
    std::vector<MoodEntry> entries;
};
/// @brief Последние расчёты и статистика прогнозов маршрутов.
struct ClientRoutes
{
    std::vector<RouteCalculation> recent;
    RouteCalculationStatistics statistics;
};
/// @brief Проверенный результат чтения без Qt и JSON.
using ClientReadResult = std::variant<ClientSnapshot, ClientChatList, ClientMessagePage, ClientCalendarDay,
                                      ClientCalendarMonth, ClientProfile, ClientMoods, ClientRoutes>;

/// @brief Действие пользователя по установке завершённости задачи.
struct ClientSetTaskDone
{
    std::int64_t taskIdentifier{};
    bool done{false};
};
/// @brief Явная отправка полного сообщения и необязательной поисковой фразы.
struct ClientSendMessage
{
    std::int64_t chatIdentifier{};
    std::string text;
    std::optional<std::string> searchQuery{};
};
/// @brief Только явный повтор с ожидаемым ID исходного User.
struct ClientRetryMessage
{
    std::int64_t chatIdentifier{};
    std::int64_t userMessageIdentifier{};
    std::optional<std::string> searchQuery{};
};
/// @brief Создание беседы без изменения экранов других устройств.
struct ClientCreateChat
{
};
/// @brief Действия, уже предоставляемые общими моделями; остальные формы добавляются с экранами.
using ClientAction = std::variant<ClientSetTaskDone, ClientSendMessage, ClientRetryMessage, ClientCreateChat,
                                  ClientPlannerAction>;

} // namespace msa::core
