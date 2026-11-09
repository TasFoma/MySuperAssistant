#include "msa/core/EventService.h"

#include "msa/core/detail/Utf8Text.h"

namespace msa::core {
namespace {

constexpr auto kDayDuration = std::chrono::hours{24};

[[nodiscard]] std::optional<EventRejection> validate(const std::string& title,
    std::chrono::year_month_day date, std::optional<std::chrono::minutes> time,
    EventRepeat repeat)
{
    if (text::charCount(title) < 1) {
        return EventRejection{"Введите название события."};
    }
    if (!date.ok()) {
        return EventRejection{"Укажите корректную дату события."};
    }
    if (time && (*time < std::chrono::minutes::zero() || *time >= kDayDuration)) {
        return EventRejection{"Укажите время от 00:00 до 23:59."};
    }
    if (repeat != EventRepeat::None && repeat != EventRepeat::Daily
        && repeat != EventRepeat::Weekly && repeat != EventRepeat::Monthly) {
        return EventRejection{"Выберите допустимый повтор события."};
    }

    return std::nullopt;
}

}  // namespace

EventService::EventService(EventRepository& repository) : m_repository{repository}
{
}

EventWriteResult EventService::addEvent(const std::string& title,
    std::chrono::year_month_day date, std::optional<std::chrono::minutes> time,
    const std::string& description, EventRepeat repeat)
{
    if (const auto rejection = validate(title, date, time, repeat)) {
        return *rejection;
    }

    return m_repository.add(Event{.title = title, .date = date, .time = time,
                                  .description = description, .repeat = repeat});
}

EventChangeResult EventService::updateEvent(std::int64_t identifier, const std::string& title,
    std::chrono::year_month_day date, std::optional<std::chrono::minutes> time,
    const std::string& description, EventRepeat repeat)
{
    if (const auto rejection = validate(title, date, time, repeat)) {
        return *rejection;
    }
    if (!m_repository.update(Event{.identifier = identifier, .title = title, .date = date,
                                   .time = time, .description = description, .repeat = repeat})) {
        return EventRejection{"Событие не найдено."};
    }

    return std::monostate{};
}

}  // namespace msa::core

