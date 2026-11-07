#include <msa/infra/ProtocolPayload.h>

#include <QDate>
#include <QJsonArray>
#include <QStringDecoder>
#include <QTime>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace msa::infra
{
namespace
{
constexpr int kMinutesPerHour{60};
constexpr int kMinutesPerDay{1440};
constexpr double kLatitudeLimit{90.0};
constexpr double kLongitudeLimit{180.0};
constexpr int kMaximumYear{9999};

void require(bool condition)
{
    if (!condition)
    {
        throw std::invalid_argument("Некорректные данные результата протокола.");
    }
}

QString text(const std::string& value)
{
    auto decoder = QStringDecoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString result = decoder(QByteArrayView(value.data(), static_cast<qsizetype>(value.size())));
    require(!decoder.hasError());

    return result;
}

QString enumName(int value, std::initializer_list<const char*> names)
{
    require(value >= 0 && static_cast<std::size_t>(value) < names.size());

    return QString::fromLatin1(*(names.begin() + value));
}

QJsonValue optionalId(const std::optional<std::int64_t>& value)
{
    return value ? QJsonValue(ProtocolPayload::identifier(*value)) : QJsonValue(QJsonValue::Null);
}

double number(double value)
{
    require(std::isfinite(value));

    return value;
}

QJsonValue optionalNumber(const std::optional<double>& value)
{
    return value ? QJsonValue(number(*value)) : QJsonValue(QJsonValue::Null);
}
} // namespace

QString ProtocolPayload::identifier(std::int64_t value)
{
    require(value > 0);

    return QString::number(value);
}

QString ProtocolPayload::date(std::chrono::year_month_day value)
{
    const auto year = static_cast<int>(value.year());
    require(value.ok() && year >= 1 && year <= kMaximumYear);

    return QDate(year, static_cast<int>(static_cast<unsigned>(value.month())),
                 static_cast<int>(static_cast<unsigned>(value.day())))
        .toString("yyyy-MM-dd");
}

QString ProtocolPayload::localDateTime(std::chrono::local_seconds value)
{
    const auto days = std::chrono::floor<std::chrono::days>(value);
    require(days.time_since_epoch() >= std::chrono::sys_days{std::chrono::year{1} / 1 / 1}.time_since_epoch() &&
            days.time_since_epoch() <=
                std::chrono::sys_days{std::chrono::year{kMaximumYear} / 12 / 31}.time_since_epoch());
    const auto calendar = std::chrono::year_month_day{std::chrono::sys_days{days.time_since_epoch()}};
    const auto dayText = date(calendar);
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(value - days).count();

    return dayText + 'T' + QTime::fromMSecsSinceStartOfDay(static_cast<int>(seconds * 1000)).toString("HH:mm:ss");
}

QJsonObject ProtocolPayload::object(const core::GeoCoordinates& value)
{
    require(std::abs(number(value.latitude)) <= kLatitudeLimit && std::abs(number(value.longitude)) <= kLongitudeLimit);

    return {{"latitude", value.latitude}, {"longitude", value.longitude}};
}

QJsonObject ProtocolPayload::object(const core::Task& value)
{
    return {{"id", identifier(value.identifier)},
            {"title", text(value.title)},
            {"due_date", value.dueDate ? QJsonValue(date(*value.dueDate)) : QJsonValue(QJsonValue::Null)},
            {"priority", enumName(static_cast<int>(value.priority), {"Low", "Normal", "High"})},
            {"done", value.done},
            {"created_at", localDateTime(value.createdAt)}};
}

QJsonObject ProtocolPayload::object(const core::Event& value)
{
    QJsonValue eventTime(QJsonValue::Null);
    if (value.time)
    {
        const auto minutes = value.time->count();
        require(minutes >= 0 && minutes < kMinutesPerDay);
        eventTime = QTime(static_cast<int>(minutes / kMinutesPerHour), static_cast<int>(minutes % kMinutesPerHour))
                        .toString("HH:mm");
    }

    return {{"id", identifier(value.identifier)},
            {"title", text(value.title)},
            {"date", date(value.date)},
            {"time", eventTime},
            {"description", text(value.description)},
            {"repeat", enumName(static_cast<int>(value.repeat), {"None", "Daily", "Weekly", "Monthly"})}};
}

QJsonObject ProtocolPayload::occurrence(const core::Event& value, std::chrono::year_month_day occurrenceDate)
{
    return {{"event", object(value)}, {"occurrence_date", date(occurrenceDate)}};
}

QJsonObject ProtocolPayload::object(const core::Habit& value)
{
    require(number(value.target) > 0.0);

    return {{"id", identifier(value.identifier)},
            {"name", text(value.name)},
            {"target", value.target},
            {"unit", text(value.unit)},
            {"period", enumName(static_cast<int>(value.period), {"Day", "Week"})}};
}

QJsonObject ProtocolPayload::object(const core::HabitProgress& value)
{
    require(number(value.progress) >= 0.0);

    return {{"habit", object(value.habit)}, {"progress", value.progress}};
}

QJsonObject ProtocolPayload::object(const core::Reminder& value)
{
    return std::visit(
        [](const auto& reminder) -> QJsonObject
        {
            using ReminderType = std::decay_t<decltype(reminder)>;
            QJsonObject result{{"id", identifier(reminder.identifier)}, {"title", text(reminder.title)}};
            if constexpr (std::is_same_v<ReminderType, core::PlaceReminder>)
            {
                result.insert("kind", "Place");
                result.insert("place", text(reminder.place));
                result.insert("coordinates", reminder.coordinates ? QJsonValue(object(*reminder.coordinates))
                                                                  : QJsonValue(QJsonValue::Null));
            }
            else
            {
                result.insert("due_at", localDateTime(reminder.dueAt));
                if constexpr (std::is_same_v<ReminderType, core::RecurringReminder>)
                {
                    require(reminder.interval.count() > 0 &&
                            reminder.interval.count() <= std::numeric_limits<int>::max());
                    result.insert("kind", "Recurring");
                    result.insert("interval_minutes", static_cast<int>(reminder.interval.count()));
                }
                else
                {
                    result.insert("kind", "Timed");
                }
            }
            if constexpr (!std::is_same_v<ReminderType, core::RecurringReminder>)
            {
                result.insert("status", enumName(static_cast<int>(reminder.status), {"Pending", "Fired", "Missed"}));
            }

            return result;
        },
        value);
}

QJsonObject ProtocolPayload::object(const core::Chat& value)
{
    return {{"id", identifier(value.identifier)}, {"title", text(value.title)}};
}

QJsonObject ProtocolPayload::object(const core::ChatMessage& value)
{
    return {{"id", identifier(value.identifier)},
            {"chat_id", identifier(value.chatIdentifier)},
            {"role", enumName(static_cast<int>(value.role), {"User", "Assistant"})},
            {"text", text(value.text)},
            {"created_at", localDateTime(value.createdAt)}};
}

QJsonObject ProtocolPayload::object(const core::MoodEntry& value)
{
    return {{"id", identifier(value.identifier)},
            {"recorded_at", localDateTime(value.recordedAt)},
            {"score", enumName(static_cast<int>(value.score) + 1, {"Negative", "Neutral", "Positive"})},
            {"note", text(value.note)}};
}

QJsonObject ProtocolPayload::object(const core::ProfileFactSource& value)
{
    return {{"chat_id", identifier(value.chatIdentifier)},
            {"message_id", identifier(value.messageIdentifier)},
            {"mood_id", optionalId(value.moodIdentifier)}};
}

QJsonObject ProtocolPayload::object(const core::UserProfileFact& value)
{
    QJsonArray tags;
    for (const auto& tag : value.tags)
    {
        tags.append(text(tag));
    }

    return {{"id", identifier(value.identifier)},
            {"text", text(value.text)},
            {"tags", tags},
            {"always_include", value.alwaysInclude},
            {"source", value.source ? QJsonValue(object(*value.source)) : QJsonValue(QJsonValue::Null)},
            {"replaces_fact_id", optionalId(value.replacesFactIdentifier)},
            {"superseded_by_fact_id", optionalId(value.supersededByFactIdentifier)}};
}

QJsonObject ProtocolPayload::object(const core::WebSearchResult& value)
{
    return {{"title", text(value.title)}, {"url", text(value.url)}, {"snippet", text(value.snippet)}};
}

QJsonObject ProtocolPayload::object(const core::RouteCalculation& value)
{
    require(number(value.distanceMeters) >= 0.0 && number(value.durationSeconds) >= 0.0);

    return {{"id", identifier(value.identifier)},
            {"created_at", localDateTime(value.createdAt)},
            {"origin_name", text(value.originName)},
            {"destination_name", text(value.destinationName)},
            {"origin", object(value.origin)},
            {"destination", object(value.destination)},
            {"mode", enumName(static_cast<int>(value.mode), {"Foot", "Driving", "Bike"})},
            {"distance_meters", value.distanceMeters},
            {"duration_seconds", value.durationSeconds},
            {"approximate", value.approximate}};
}

QJsonObject ProtocolPayload::object(const core::RouteStep& value)
{
    require(number(value.distanceMeters) >= 0.0 && (!value.exitNumber || *value.exitNumber > 0));

    return {{"type", text(value.type)},
            {"modifier", text(value.modifier)},
            {"street", text(value.street)},
            {"distance_meters", value.distanceMeters},
            {"exit_number", value.exitNumber ? QJsonValue(*value.exitNumber) : QJsonValue(QJsonValue::Null)}};
}

QJsonObject ProtocolPayload::object(const core::RouteCalculationStatistics& value)
{
    constexpr std::size_t kMaximumExactJsonInteger{9007199254740991ULL};
    require(value.routedCount <= kMaximumExactJsonInteger && value.approximateCount <= kMaximumExactJsonInteger);
    require(number(value.plannedDistanceMeters) >= 0.0 && number(value.plannedDurationSeconds) >= 0.0);

    return {{"routed_count", static_cast<qint64>(value.routedCount)},
            {"approximate_count", static_cast<qint64>(value.approximateCount)},
            {"planned_distance_meters", value.plannedDistanceMeters},
            {"planned_duration_seconds", value.plannedDurationSeconds}};
}

QJsonObject ProtocolPayload::object(const core::WeatherForecast& value)
{
    constexpr int kMaximumPercent{100};
    require(value.maximumTemperatureCelsius.has_value() == value.minimumTemperatureCelsius.has_value());
    require(!value.maximumTemperatureCelsius || *value.maximumTemperatureCelsius >= *value.minimumTemperatureCelsius);
    require(value.weatherCode >= 0 && number(value.windSpeedKilometersPerHour) >= 0.0);
    require(!value.precipitationProbabilityPercent ||
            (*value.precipitationProbabilityPercent >= 0 && *value.precipitationProbabilityPercent <= kMaximumPercent));

    return {{"temperature_celsius", number(value.temperatureCelsius)},
            {"apparent_temperature_celsius", number(value.apparentTemperatureCelsius)},
            {"weather_code", value.weatherCode},
            {"wind_speed_kilometers_per_hour", value.windSpeedKilometersPerHour},
            {"maximum_temperature_celsius", optionalNumber(value.maximumTemperatureCelsius)},
            {"minimum_temperature_celsius", optionalNumber(value.minimumTemperatureCelsius)},
            {"precipitation_probability_percent", value.precipitationProbabilityPercent
                                                      ? QJsonValue(*value.precipitationProbabilityPercent)
                                                      : QJsonValue(QJsonValue::Null)}};
}
} // namespace msa::infra
