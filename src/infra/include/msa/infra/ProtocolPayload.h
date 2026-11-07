#pragma once

#include <msa/core/Chat.h>
#include <msa/core/ChatMessage.h>
#include <msa/core/Event.h>
#include <msa/core/HabitService.h>
#include <msa/core/MoodEntry.h>
#include <msa/core/Reminder.h>
#include <msa/core/RouteHistoryService.h>
#include <msa/core/Task.h>
#include <msa/core/UserProfile.h>
#include <msa/core/WeatherProvider.h>
#include <msa/core/WebSearchProvider.h>

#include <QJsonObject>
#include <QString>

#include <chrono>
#include <cstdint>

namespace msa::infra
{
/// Представление сохранённых объектов core в JSON версии 1. Некорректные значения
/// отвергаются std::invalid_argument до отправки; ID не превращаются в JSON-числа.
class ProtocolPayload
{
public:
    /// Положительный ID; ноль несохранённой записи запрещён.
    [[nodiscard]] static QString identifier(std::int64_t value);
    /// Календарная дата без часового пояса.
    [[nodiscard]] static QString date(std::chrono::year_month_day value);
    /// Местные секунды без преобразования в UTC или системную зону клиента.
    [[nodiscard]] static QString localDateTime(std::chrono::local_seconds value);
    /// Координаты, включая допустимые нули.
    [[nodiscard]] static QJsonObject object(const core::GeoCoordinates& value);
    /// Сохранённая задача с явным nullable сроком.
    [[nodiscard]] static QJsonObject object(const core::Task& value);
    /// Исходная серия события, без подмены даты вхождением.
    [[nodiscard]] static QJsonObject object(const core::Event& value);
    /// Вхождение серии на конкретную дату.
    [[nodiscard]] static QJsonObject occurrence(const core::Event& value, std::chrono::year_month_day occurrenceDate);
    /// Привычка с количественной целью.
    [[nodiscard]] static QJsonObject object(const core::Habit& value);
    /// Текущий прогресс без ограничения целью.
    [[nodiscard]] static QJsonObject object(const core::HabitProgress& value);
    /// Один из трёх видов напоминаний.
    [[nodiscard]] static QJsonObject object(const core::Reminder& value);
    /// Беседа без глобального признака выбора.
    [[nodiscard]] static QJsonObject object(const core::Chat& value);
    /// Полный текст сообщения без обрезки.
    [[nodiscard]] static QJsonObject object(const core::ChatMessage& value);
    /// Запись дневника с полной заметкой.
    [[nodiscard]] static QJsonObject object(const core::MoodEntry& value);
    /// Источник факта профиля.
    [[nodiscard]] static QJsonObject object(const core::ProfileFactSource& value);
    /// Факт и nullable ссылки на источник и версии.
    [[nodiscard]] static QJsonObject object(const core::UserProfileFact& value);
    /// Переданный модели источник поиска.
    [[nodiscard]] static QJsonObject object(const core::WebSearchResult& value);
    /// Сохранённый расчёт маршрута.
    [[nodiscard]] static QJsonObject object(const core::RouteCalculation& value);
    /// Манёвр с явным nullable номером съезда.
    [[nodiscard]] static QJsonObject object(const core::RouteStep& value);
    /// Итоги расчётов, не статистика совершённых поездок.
    [[nodiscard]] static QJsonObject object(const core::RouteCalculationStatistics& value);
    /// Прогноз без строк единиц измерения.
    [[nodiscard]] static QJsonObject object(const core::WeatherForecast& value);
};
} // namespace msa::infra
