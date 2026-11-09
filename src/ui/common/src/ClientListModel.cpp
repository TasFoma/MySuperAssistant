#include "msa/ui/ClientListModel.h"

#include <QDate>
#include <QLocale>
#include <QStringList>
#include <QTime>
#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <type_traits>

namespace msa::ui
{
namespace
{
QString text(const std::string& value)
{
    return QString::fromStdString(value);
}
QString identifier(std::int64_t value)
{
    return QString::number(value);
}
QString date(std::chrono::year_month_day value)
{
    return QDate{static_cast<int>(value.year()), static_cast<int>(static_cast<unsigned>(value.month())),
                 static_cast<int>(static_cast<unsigned>(value.day()))}
        .toString(Qt::ISODate);
}
QString moment(std::chrono::local_seconds value)
{
    const auto start = std::chrono::floor<std::chrono::days>(value);
    const auto day = std::chrono::year_month_day{std::chrono::sys_days{start.time_since_epoch()}};
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(value - start).count();
    return date(day) + 'T' + QTime::fromMSecsSinceStartOfDay(static_cast<int>(elapsed * 1000)).toString("HH:mm:ss");
}
QString enumText(int value, std::initializer_list<const char*> names)
{
    int position{0};
    for (const auto* name : names)
    {
        if (position++ == value)
        {
            return QString::fromUtf8(name);
        }
    }
    return {};
}
QVariant optionalId(const std::optional<std::int64_t>& value)
{
    return value ? QVariant{identifier(*value)} : QVariant{};
}
QVariantMap row(const core::Task& value)
{
    return {{"identifier", identifier(value.identifier)},
            {"title", text(value.title)},
            {"dueDate", value.dueDate ? QVariant{date(*value.dueDate)} : QVariant{}},
            {"priority", enumText(static_cast<int>(value.priority), {"Low", "Normal", "High"})},
            {"done", value.done},
            {"createdAt", moment(value.createdAt)},
            {"detailsText", (value.dueDate ? date(*value.dueDate) : QString{"Без срока"}) + " · " +
                                enumText(static_cast<int>(value.priority),
                                         {"Низкий приоритет", "Обычный приоритет", "Высокий приоритет"})}};
}
template <typename ReminderType> QVariantMap reminder(const ReminderType& value)
{
    QVariantMap result{{"identifier", identifier(value.identifier)}, {"title", text(value.title)}};
    if constexpr (std::is_same_v<ReminderType, core::PlaceReminder>)
    {
        result["kind"] = "Place";
        result["place"] = text(value.place);
        result["hasCoordinates"] = value.coordinates.has_value();
        result["latitude"] = value.coordinates ? QVariant{value.coordinates->latitude} : QVariant{};
        result["longitude"] = value.coordinates ? QVariant{value.coordinates->longitude} : QVariant{};
    }
    else
    {
        result["dueAt"] = moment(value.dueAt);
        if constexpr (std::is_same_v<ReminderType, core::RecurringReminder>)
        {
            result["kind"] = "Recurring";
            result["intervalMinutes"] = QVariant::fromValue<qlonglong>(value.interval.count());
        }
        else
        {
            result["kind"] = "Timed";
        }
    }
    if constexpr (!std::is_same_v<ReminderType, core::RecurringReminder>)
    {
        result["status"] = enumText(static_cast<int>(value.status), {"Pending", "Fired", "Missed"});
    }
    const auto kind = result.value("kind").toString();
    QString details;
    if (kind == "Place")
    {
        details = "По месту: " + result.value("place").toString();
    }
    else
    {
        details = result.value("dueAt").toString();
        if (kind == "Recurring")
        {
            details += " · повтор через " + result.value("intervalMinutes").toString() + " мин.";
        }
    }
    const auto status = result.value("status").toString();
    if (status == "Fired")
    {
        details += " · выполнено";
    }
    if (status == "Missed")
    {
        details += " · пропущено";
    }
    if (status == "Pending")
    {
        details += " · ожидает";
    }
    result["detailsText"] = details;
    return result;
}
QVariantMap row(const core::TimedReminder& value)
{
    return reminder(value);
}
QVariantMap row(const core::RecurringReminder& value)
{
    return reminder(value);
}
QVariantMap row(const core::PlaceReminder& value)
{
    return reminder(value);
}
QVariantMap row(const core::HabitProgress& value)
{
    return {{"identifier", identifier(value.habit.identifier)},
            {"name", text(value.habit.name)},
            {"target", value.habit.target},
            {"unit", text(value.habit.unit)},
            {"period", enumText(static_cast<int>(value.habit.period), {"Day", "Week"})},
            {"progress", value.progress},
            {"detailsText", QLocale{}.toString(value.progress) + " / " + QLocale{}.toString(value.habit.target) + " " +
                                text(value.habit.unit)}};
}
QVariantMap row(const core::ClientEventOccurrence& value)
{
    const auto& event = value.event;
    return {
        {"identifier", identifier(event.identifier)},
        {"title", text(event.title)},
        {"date", date(event.date)},
        {"time", event.time ? QVariant{QTime::fromMSecsSinceStartOfDay(static_cast<int>(event.time->count() * 60000))
                                           .toString("HH:mm")}
                            : QVariant{}},
        {"description", text(event.description)},
        {"repeat", enumText(static_cast<int>(event.repeat), {"None", "Daily", "Weekly", "Monthly"})},
        {"occurrenceDate", date(value.occurrenceDate)},
        {"detailsText",
         date(value.occurrenceDate) +
             (event.time
                  ? " · " +
                        QTime::fromMSecsSinceStartOfDay(static_cast<int>(event.time->count() * 60000)).toString("HH:mm")
                  : QString{}) +
             "\n" + text(event.description)}};
}
QVariantMap row(const core::Chat& value)
{
    return {{"identifier", identifier(value.identifier)}, {"title", text(value.title)}};
}
QVariantMap row(const core::ChatMessage& value)
{
    return {{"identifier", identifier(value.identifier)},
            {"chatIdentifier", identifier(value.chatIdentifier)},
            {"role", enumText(static_cast<int>(value.role), {"User", "Assistant"})},
            {"text", text(value.text)},
            {"createdAt", moment(value.createdAt)},
            {"detailsText", value.role == core::MessageRole::User ? QString{"Вы"} : QString{"Ассистент"}}};
}
QVariantMap row(const core::UserProfileFact& value)
{
    QStringList tags;
    for (const auto& tag : value.tags)
    {
        tags.push_back(text(tag));
    }
    return {
        {"identifier", identifier(value.identifier)},
        {"text", text(value.text)},
        {"tags", tags},
        {"alwaysInclude", value.alwaysInclude},
        {"sourceChatIdentifier", value.source ? QVariant{identifier(value.source->chatIdentifier)} : QVariant{}},
        {"sourceMessageIdentifier", value.source ? QVariant{identifier(value.source->messageIdentifier)} : QVariant{}},
        {"sourceMoodIdentifier", value.source ? optionalId(value.source->moodIdentifier) : QVariant{}},
        {"replacesFactIdentifier", optionalId(value.replacesFactIdentifier)},
        {"supersededByFactIdentifier", optionalId(value.supersededByFactIdentifier)}};
}
QVariantMap row(const core::MoodEntry& value)
{
    return {{"identifier", identifier(value.identifier)},
            {"recordedAt", moment(value.recordedAt)},
            {"score", enumText(static_cast<int>(value.score) + 1, {"Negative", "Neutral", "Positive"})},
            {"text", text(value.note)}};
}
QVariantMap row(const core::RouteCalculation& value)
{
    return {{"identifier", identifier(value.identifier)},
            {"createdAt", moment(value.createdAt)},
            {"originName", text(value.originName)},
            {"destinationName", text(value.destinationName)},
            {"originLatitude", value.origin.latitude},
            {"originLongitude", value.origin.longitude},
            {"destinationLatitude", value.destination.latitude},
            {"destinationLongitude", value.destination.longitude},
            {"travelMode", enumText(static_cast<int>(value.mode), {"Foot", "Driving", "Bike"})},
            {"distanceMeters", value.distanceMeters},
            {"durationSeconds", value.durationSeconds},
            {"approximate", value.approximate}};
}
const QHash<int, QByteArray>& roles()
{
    static const auto namesByRole = []()
    {
        const auto names = std::to_array<const char*>({"identifier",
                                                       "title",
                                                       "text",
                                                       "dueDate",
                                                       "priority",
                                                       "done",
                                                       "createdAt",
                                                       "kind",
                                                       "status",
                                                       "dueAt",
                                                       "intervalMinutes",
                                                       "place",
                                                       "hasCoordinates",
                                                       "latitude",
                                                       "longitude",
                                                       "name",
                                                       "target",
                                                       "unit",
                                                       "period",
                                                       "progress",
                                                       "date",
                                                       "time",
                                                       "description",
                                                       "repeat",
                                                       "occurrenceDate",
                                                       "chatIdentifier",
                                                       "role",
                                                       "tags",
                                                       "alwaysInclude",
                                                       "sourceChatIdentifier",
                                                       "sourceMessageIdentifier",
                                                       "sourceMoodIdentifier",
                                                       "replacesFactIdentifier",
                                                       "supersededByFactIdentifier",
                                                       "recordedAt",
                                                       "score",
                                                       "originName",
                                                       "destinationName",
                                                       "originLatitude",
                                                       "originLongitude",
                                                       "destinationLatitude",
                                                       "destinationLongitude",
                                                       "travelMode",
                                                       "distanceMeters",
                                                       "durationSeconds",
                                                       "approximate",
                                                       "detailsText"});
        QHash<int, QByteArray> result;
        int role = Qt::UserRole + 1;
        for (const auto* name : names)
        {
            result.insert(role++, name);
        }
        return result;
    }();
    return namesByRole;
}
} // namespace

ClientListModel::ClientListModel(ClientListKind kind, QObject* parent) : QAbstractListModel{parent}, m_kind{kind} {}
int ClientListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}
int ClientListModel::count() const
{
    return static_cast<int>(m_rows.size());
}
bool ClientListModel::stale() const
{
    return m_stale;
}
QHash<int, QByteArray> ClientListModel::roleNames() const
{
    return roles();
}
QVariant ClientListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= count())
    {
        return {};
    }
    const auto name = roles().value(role);
    if (name.isEmpty())
    {
        return {};
    }
    return m_rows[static_cast<std::size_t>(index.row())].value(QString::fromLatin1(name));
}
void ClientListModel::setStale(bool stale)
{
    if (m_stale == stale)
    {
        return;
    }
    m_stale = stale;
    emit staleChanged();
}
bool ClientListModel::accepts(const std::vector<ClientListRow>& rows) const
{
    if (rows.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }
    std::set<QString> identifiers;
    for (const auto& item : rows)
    {
        const auto kind = std::visit(
            [](const auto& value)
            {
                using RowType = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<RowType, core::Task>)
                {
                    return ClientListKind::Tasks;
                }
                else if constexpr (std::is_same_v<RowType, core::TimedReminder> ||
                                   std::is_same_v<RowType, core::RecurringReminder> ||
                                   std::is_same_v<RowType, core::PlaceReminder>)
                {
                    return ClientListKind::Reminders;
                }
                else if constexpr (std::is_same_v<RowType, core::HabitProgress>)
                {
                    return ClientListKind::Habits;
                }
                else if constexpr (std::is_same_v<RowType, core::ClientEventOccurrence>)
                {
                    return ClientListKind::Events;
                }
                else if constexpr (std::is_same_v<RowType, core::Chat>)
                {
                    return ClientListKind::Chats;
                }
                else if constexpr (std::is_same_v<RowType, core::ChatMessage>)
                {
                    return ClientListKind::Messages;
                }
                else if constexpr (std::is_same_v<RowType, core::UserProfileFact>)
                {
                    return ClientListKind::Profile;
                }
                else if constexpr (std::is_same_v<RowType, core::MoodEntry>)
                {
                    return ClientListKind::Moods;
                }
                else
                {
                    return ClientListKind::Routes;
                }
            },
            item);
        if (kind != m_kind)
        {
            return false;
        }
        const auto value =
            std::visit([](const auto& entry) { return row(entry).value("identifier").toString(); }, item);
        if (value.toLongLong() <= 0 || !identifiers.insert(value).second)
        {
            return false;
        }
    }
    return true;
}
bool ClientListModel::replaceRows(const std::vector<ClientListRow>& rows, bool stale)
{
    if (!accepts(rows))
    {
        return false;
    }
    std::vector<QVariantMap> prepared;
    prepared.reserve(rows.size());
    for (const auto& item : rows)
    {
        prepared.push_back(std::visit([](const auto& value) { return row(value); }, item));
    }
    const auto previousCount = count();
    beginResetModel();
    m_rows = std::move(prepared);
    endResetModel();
    if (previousCount != count())
    {
        emit countChanged();
    }
    setStale(stale);
    return true;
}
bool ClientListModel::containsIdentifier(const QString& identifier) const
{
    return std::ranges::any_of(m_rows, [identifier](const auto& item)
                               { return item.value("identifier").toString() == identifier; });
}
} // namespace msa::ui
