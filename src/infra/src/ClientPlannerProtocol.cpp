#include "msa/infra/detail/ClientPlannerProtocol.h"

#include "msa/infra/ProtocolCodec.h"
#include "msa/infra/ProtocolPayload.h"

#include <stdexcept>
#include <type_traits>

namespace msa::infra
{
namespace
{
void require(bool condition)
{
    if (!condition)
    {
        throw std::invalid_argument{"Неверное подтверждение действия планировщика."};
    }
}
QString itemName(core::ClientItemKind kind)
{
    switch (kind)
    {
    case core::ClientItemKind::Task:
        return "task";
    case core::ClientItemKind::Event:
        return "event";
    case core::ClientItemKind::Habit:
        return "habit";
    case core::ClientItemKind::Reminder:
        return "reminder";
    }
    throw std::invalid_argument{"Неизвестный вид записи планировщика."};
}
void checkIdentifier(const QJsonObject& result, const QString& key, std::optional<std::int64_t> expected = {})
{
    require(result.size() == 1 && ProtocolCodec::isEntityId(result.value(key)));
    require(!expected || result.value(key).toString().toLongLong() == *expected);
}
} // namespace

ClientWireRequest ClientPlannerProtocol::request(const core::ClientPlannerAction& action)
{
    using Payload = ProtocolPayload;
    return std::visit(
        [](const auto& value) -> ClientWireRequest
        {
            using ActionType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<ActionType, core::ClientSaveTask>)
            {
                auto params = Payload::object(core::Task{1, value.title, value.dueDate, value.priority, false, {}});
                params.remove("id");
                params.remove("done");
                params.remove("created_at");
                if (value.identifier)
                {
                    params.insert("task_id", Payload::identifier(*value.identifier));
                }
                return {value.identifier ? "task.update" : "task.create", params};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientSaveEvent>)
            {
                auto params = Payload::object(
                    core::Event{1, value.title, value.date, value.time, value.description, value.repeat});
                params.remove("id");
                if (value.identifier)
                {
                    params.insert("event_id", Payload::identifier(*value.identifier));
                }
                return {value.identifier ? "event.update" : "event.create", params};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientCreateHabit>)
            {
                auto params = Payload::object(core::Habit{1, value.name, value.target, value.unit, value.period});
                params.remove("id");
                return {"habit.create", params};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientLogHabit>)
            {
                return {"habit.log", {{"habit_id", Payload::identifier(value.identifier)}, {"value", value.value}}};
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientDeleteItem>)
            {
                const auto name = itemName(value.kind);
                return {name + ".delete", {{name + "_id", Payload::identifier(value.identifier)}}};
            }
            else
            {
                QJsonObject params;
                if constexpr (std::is_same_v<ActionType, core::ClientCreateTimedReminder>)
                {
                    params = Payload::object(core::Reminder{core::TimedReminder{1, value.title, value.dueAt}});
                }
                else if constexpr (std::is_same_v<ActionType, core::ClientCreateRecurringReminder>)
                {
                    params =
                        Payload::object(core::Reminder{core::RecurringReminder{1, value.title, {}, value.interval}});
                    params.remove("due_at");
                }
                else
                {
                    params = Payload::object(core::Reminder{core::PlaceReminder{1, value.title, value.place}});
                    params.remove("coordinates");
                }
                params.remove("id");
                params.remove("status");
                return {"reminder.create", params};
            }
        },
        action);
}

void ClientPlannerProtocol::validateResult(const core::ClientPlannerAction& action, const QJsonObject& result)
{
    std::visit(
        [&result](const auto& value)
        {
            using ActionType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<ActionType, core::ClientSaveTask>)
            {
                checkIdentifier(result, "task_id", value.identifier);
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientSaveEvent>)
            {
                checkIdentifier(result, "event_id", value.identifier);
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientCreateHabit>)
            {
                checkIdentifier(result, "habit_id");
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientLogHabit>)
            {
                checkIdentifier(result, "log_id");
            }
            else if constexpr (std::is_same_v<ActionType, core::ClientDeleteItem>)
            {
                require(result.size() == 1 && result.value("deleted").isBool() && result.value("deleted").toBool());
            }
            else
            {
                checkIdentifier(result, "reminder_id");
            }
        },
        action);
}
} // namespace msa::infra
