#include "msa/core/detail/PlannerText.h"

#include <array>
#include <string_view>

namespace msa::core::text {

std::string russianDate(std::chrono::year_month_day date)
{
    static constexpr std::array<std::string_view, 7> kDays{
        "понедельник", "вторник", "среда", "четверг", "пятница", "суббота", "воскресенье"};
    static constexpr std::array<std::string_view, 12> kMonths{
        "января", "февраля", "марта", "апреля", "мая", "июня",
        "июля", "августа", "сентября", "октября", "ноября", "декабря"};
    const auto weekday = std::chrono::weekday{std::chrono::sys_days{date}}.iso_encoding();

    return std::string{kDays[weekday - 1]} + ", " + std::to_string(static_cast<unsigned>(date.day()))
           + " " + std::string{kMonths[static_cast<unsigned>(date.month()) - 1]}
           + " " + std::to_string(static_cast<int>(date.year()));
}

std::string timeText(std::chrono::minutes time)
{
    constexpr int kMinutesPerHour = 60;
    constexpr int kTwoDigitThreshold = 10;
    const auto hour = time.count() / kMinutesPerHour;
    const auto minute = time.count() % kMinutesPerHour;

    return (hour < kTwoDigitThreshold ? "0" : "") + std::to_string(hour) + ":"
           + (minute < kTwoDigitThreshold ? "0" : "") + std::to_string(minute);
}

}  // namespace msa::core::text

