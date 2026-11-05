#include "msa/core/CommandParser.h"

#include "Pcre2Regex.h"
#include "Utf8Text.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace msa::core {

namespace {

namespace utf8 = msa::core::text;

using LocalSeconds = std::chrono::local_time<std::chrono::seconds>;
using LocalMinutes = std::chrono::local_time<std::chrono::minutes>;
using detail::Pcre2Regex;

constexpr std::size_t kMinTitleLength = 3;
constexpr std::size_t kMinListItems = 2;
constexpr std::size_t kMinListItemLength = 3;
constexpr int kMinutesPerHour = 60;
constexpr int kMinutesPerDay = 24 * kMinutesPerHour;
constexpr int kHalfHourMinutes = 30;
constexpr int kMorningHour = 9;
constexpr int kMaxHour = 23;
constexpr int kMaxMinute = 59;
constexpr int kDefaultRelativeNumber = 1;
constexpr int kTomorrowOffset = 1;
constexpr int kDayAfterTomorrowOffset = 2;
constexpr int kDaysInWeek = 7;

/// @brief Срок или время, извлечённые из текста. Пустые поля — «не указано».
struct When {
    std::optional<std::chrono::year_month_day> date;
    std::optional<std::chrono::minutes> time;  // от полуночи
    std::string cleaned;                       // текст без временного выражения
};

/// @brief Заменяет все совпадения regex на replacement.
[[nodiscard]] std::string replaceAll(std::string_view subject, const Pcre2Regex& regex,
                                     std::string_view replacement)
{
    std::string result;
    std::size_t last = 0;
    for (const auto& found : regex.matchAll(subject)) {
        result.append(subject.substr(last, found.start - last));
        result.append(replacement);
        last = found.end;
    }
    result.append(subject.substr(last));
    return result;
}

/// @brief Убирает пробелы по краям строки.
[[nodiscard]] std::string trimmedText(std::string_view text)
{
    static const Pcre2Regex kEdgeSpaces{R"(^\s+|\s+$)"};
    return replaceAll(text, kEdgeSpaces, "");
}

/// @brief Читает целое из цифр. При ошибке возвращает fallback.
[[nodiscard]] int parseInt(std::string_view digits, int fallback)
{
    int value = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec != std::errc{}) {
        return fallback;
    }
    return value;
}

/// @brief Числительные словами: «пять минут», «пару часов».
[[nodiscard]] int numberWord(std::string_view word)
{
    static const std::map<std::string, int, std::less<>> kWords{
        {"одну", 1},      {"две", 2},        {"пару", 2},     {"три", 3},
        {"четыре", 4},    {"пять", 5},       {"шесть", 6},    {"семь", 7},
        {"восемь", 8},    {"девять", 9},     {"десять", 10},  {"пятнадцать", 15},
        {"двадцать", 20}, {"тридцать", 30},  {"сорок", 40},   {"пятьдесят", 50},
    };
    const auto found = kWords.find(word);
    if (found == kWords.end()) {
        return kDefaultRelativeNumber;
    }
    return found->second;
}

/// @brief Прибавляет дни к дате.
[[nodiscard]] std::chrono::year_month_day addDays(std::chrono::year_month_day day, int count)
{
    return std::chrono::year_month_day{std::chrono::sys_days{day} + std::chrono::days{count}};
}

/// @brief Календарная дата местного момента времени.
[[nodiscard]] std::chrono::year_month_day dayOf(LocalSeconds moment)
{
    const auto days = std::chrono::floor<std::chrono::days>(moment).time_since_epoch();
    return std::chrono::year_month_day{std::chrono::sys_days{days}};
}

/// @brief ISO-номер дня недели: 1 — понедельник, 7 — воскресенье.
[[nodiscard]] int isoWeekday(std::chrono::year_month_day day)
{
    return static_cast<int>(std::chrono::weekday{std::chrono::sys_days{day}}.iso_encoding());
}

/// @brief Убирает служебные слова, лишние пробелы и знаки в конце; первая буква — заглавная.
[[nodiscard]] std::string tidy(std::string_view input)
{
    static const Pcre2Regex kDoubleSpaces{R"(\s{2,})"};
    static const Pcre2Regex kLeadWords{R"(^(?:мне\s+|о\s+том,?\s+что\s+|что\s+(?:надо|нужно)\s+))"};

    std::string s = trimmedText(replaceAll(input, kDoubleSpaces, " "));
    while (!s.empty() && (s.back() == ',' || s.back() == '.' || s.back() == '?')) {
        s.pop_back();
    }
    s = replaceAll(s, kLeadWords, "");
    s = utf8::upperFirst(s);
    return trimmedText(s);
}

/// @brief Вынимает из текста «через N …», день недели, «сегодня/завтра» и время.
[[nodiscard]] When extractWhen(std::string_view text, LocalSeconds now)
{
    When w;
    w.cleaned = std::string(text);
    const auto today = dayOf(now);

    // «через N минут», «через минуту», «через полчаса», «через пару/пять минут»
    static const Pcre2Regex kRelative{
        R"(через\s+(?:([0-9]+)|(одну|две|пару|три|четыре|пять|шесть|семь|восемь|девять|десять|пятнадцать|двадцать|тридцать|сорок|пятьдесят))?\s*(полчаса|минут\w*|час\w*|дн(?:я|ей)|день))"};
    if (const auto rel = kRelative.match(w.cleaned)) {
        int count = kDefaultRelativeNumber;
        if (!rel->groups[1].empty()) {
            count = parseInt(rel->groups[1], kDefaultRelativeNumber);
        } else if (!rel->groups[2].empty()) {
            count = numberWord(utf8::toLower(rel->groups[2]));
        }
        const std::string unit = utf8::toLower(rel->groups[3]);

        LocalSeconds moment = now;
        if (unit == "полчаса") {
            moment += std::chrono::minutes{kHalfHourMinutes};
        } else if (unit.starts_with("минут")) {
            moment += std::chrono::minutes{count};
        } else if (unit.starts_with("час")) {
            moment += std::chrono::hours{count};
        } else {
            moment += std::chrono::minutes{count * kMinutesPerDay};
        }

        const auto momentDay = std::chrono::floor<std::chrono::days>(moment);
        w.date = dayOf(moment);
        w.time = std::chrono::floor<std::chrono::minutes>(moment - momentDay);
        w.cleaned.erase(rel->start, rel->end - rel->start);
        return w;
    }

    // Дни недели: «в пятницу», «во вторник» (ближайший будущий; сегодняшний день недели — сегодня)
    static const Pcre2Regex kWeekday{
        R"(\b(?:в|во)\s+(понедельник|вторник|среду|четверг|пятницу|субботу|воскресенье)\b)"};
    if (const auto wd = kWeekday.match(w.cleaned)) {
        static const std::array<std::string_view, kDaysInWeek> kNames{
            "понедельник", "вторник", "среду", "четверг", "пятницу", "субботу", "воскресенье"};
        const std::string word = utf8::toLower(wd->groups[1]);
        for (std::size_t index = 0; index < kNames.size(); ++index) {
            if (kNames[index] != word) {
                continue;
            }
            int diff = static_cast<int>(index) + 1 - isoWeekday(today);
            if (diff < 0) {
                diff += kDaysInWeek;
            }
            w.date = addDays(today, diff);
            w.cleaned.erase(wd->start, wd->end - wd->start);
            break;
        }
    }

    // «сегодня», «завтра», «послезавтра» (возможно с «на»)
    static const Pcre2Regex kDay{R"(\b(?:на\s+)?(послезавтра|завтра|сегодня)\b)"};
    if (const auto day = kDay.match(w.cleaned)) {
        const std::string word = utf8::toLower(day->groups[1]);
        int offset = 0;
        if (word == "завтра") {
            offset = kTomorrowOffset;
        } else if (word == "послезавтра") {
            offset = kDayAfterTomorrowOffset;
        }
        w.date = addDays(today, offset);
        w.cleaned.erase(day->start, day->end - day->start);
    }

    // Время: «в 15:00», «в 15.30», «в 12 51» (голосовой ввод), «в 15 часов», «к 9»
    static const Pcre2Regex kTime{
        R"(\b(?:в|к)\s*([0-9]{1,2})(?:[:.]([0-9]{2})|\s+([0-9]{2})(?![0-9]))?\s*(?:час(?:а|ов)?)?\b)"};
    if (const auto tm = kTime.match(w.cleaned)) {
        const int hour = parseInt(tm->groups[1], 0);
        const std::string minuteText = !tm->groups[2].empty() ? tm->groups[2] : tm->groups[3];
        const int minute = parseInt(minuteText, 0);
        if (hour >= 0 && hour <= kMaxHour && minute >= 0 && minute <= kMaxMinute) {
            w.time = std::chrono::minutes{hour * kMinutesPerHour + minute};
            w.cleaned.erase(tm->start, tm->end - tm->start);
        }
    }

    return w;
}

/// @brief Срезает вводные слова и просьбы вроде «привет, …» и «можешь …».
[[nodiscard]] std::string stripLeadingPhrases(std::string_view input)
{
    static const Pcre2Regex kGreeting{
        R"(^(?:привет|здравствуй(?:те)?|добро(?:е|го)\s+утр\w+|добрый\s+(?:день|вечер)|слушай|пожалуйста|будь\s+добра?|спутник|эй|окей|ладно|кстати|а)[,!.\s]+)"};
    static const Pcre2Regex kPolite{R"(^(?:можешь|могла\s+бы|не\s+забудь|давай)\s+)"};

    std::string t = trimmedText(input);
    while (const auto found = kGreeting.match(t)) {
        t.erase(0, found->end);
    }
    t = replaceAll(t, kPolite, "");
    return trimmedText(t);
}

/// @brief Напоминание о повторяющемся действии: «пить воду каждые 45 минут».
[[nodiscard]] std::optional<RecurringReminderCommand> parseRecurring(const std::string& t)
{
    static const Pcre2Regex kRecur{
        R"(кажд(?:ый|ую|ые|ое)\s*([0-9]+)?\s*(минут\w*|полчаса|час\w*|день|дня))"};
    static const Pcre2Regex kRemindWord{R"(напомин\w*|напомни\w*)"};
    static const Pcre2Regex kCleanWords{R"(\b(?:напомин\w*|напомни\w*|мне|пожалуйста|меня)\b)"};

    const auto recur = kRecur.match(t);
    if (!recur) {
        return std::nullopt;
    }
    const std::string firstWord = utf8::toLower(t.substr(0, t.find(' ')));
    const bool imperative = kRemindWord.match(t).has_value() || firstWord.ends_with("ть")
                            || firstWord.ends_with("ай");
    const std::string unit = utf8::toLower(recur->groups[2]);
    const int number = parseInt(recur->groups[1], 0);

    int minutes = 0;
    if (unit == "полчаса") {
        minutes = kHalfHourMinutes;
    } else if (unit.starts_with("минут")) {
        minutes = number;
    } else if (unit.starts_with("час")) {
        minutes = (number > 0 ? number : kDefaultRelativeNumber) * kMinutesPerHour;
    } else {
        minutes = (number > 0 ? number : kDefaultRelativeNumber) * kMinutesPerDay;
    }
    if (!imperative || minutes <= 0) {
        return std::nullopt;
    }

    std::string title = t;
    title.erase(recur->start, recur->end - recur->start);
    static const Pcre2Regex kSpaceRun{R"(\s+)"};
    title = trimmedText(replaceAll(replaceAll(title, kCleanWords, ""), kSpaceRun, " "));
    title = tidy(title);
    return RecurringReminderCommand{title, std::chrono::minutes{minutes}};
}

/// @brief Список задач: «вот задачи, которые надо сделать: 1. … 2. … 3. …».
[[nodiscard]] std::optional<TaskListCommand> parseTaskList(const std::string& t)
{
    static const Pcre2Regex kListHint{R"((задач|надо\s+сделать|нужно\s+сделать|список\s+дел))"};
    static const Pcre2Regex kNumItem{R"((?:^|\s)([0-9]{1,2})[.)]\s*)"};

    if (!kListHint.match(t)) {
        return std::nullopt;
    }
    const auto markers = kNumItem.matchAll(t);
    TaskListCommand list;
    for (std::size_t index = 0; index < markers.size(); ++index) {
        const std::size_t begin = markers[index].end;
        const std::size_t end = index + 1 < markers.size() ? markers[index + 1].start : t.size();
        const std::string item = tidy(t.substr(begin, end - begin));
        if (utf8::charCount(item) >= kMinListItemLength) {
            list.items.push_back(item);
        }
    }
    if (list.items.size() < kMinListItems) {
        return std::nullopt;
    }
    return list;
}

/// @brief Маршрут: «как добраться до библиотеки на машине».
[[nodiscard]] std::optional<RouteCommand> parseRoute(const std::string& t)
{
    static const Pcre2Regex kRouteRe{
        R"(^(?:как\s+(?:мне\s+)?(?:добраться|дойти|доехать)\s+до|построй(?:те)?\s+маршрут\s+(?:до|к)|маршрут\s+до)\s+(.+?)\??$)"};
    static const Pcre2Regex kCar{R"(на\s+(машине|авто|такси))"};
    static const Pcre2Regex kBike{R"(на\s+велосипеде)"};

    const auto route = kRouteRe.match(t);
    if (!route) {
        return std::nullopt;
    }
    std::string place = route->groups[1];
    TravelMode mode = TravelMode::Foot;
    if (kCar.match(place)) {
        place = replaceAll(place, kCar, "");
        mode = TravelMode::Driving;
    } else if (kBike.match(place)) {
        place = replaceAll(place, kBike, "");
        mode = TravelMode::Bike;
    }
    return RouteCommand{tidy(place), mode};
}

/// @brief Напоминание по месту: «напомни купить хлеб, когда буду около магазина».
[[nodiscard]] std::optional<PlaceReminderCommand> parsePlaceReminder(const std::string& t)
{
    static const Pcre2Regex kGeo{
        R"(^напомни(?:ть)?\s+(?:мне\s+)?(.+?),?\s+когда\s+(?:я\s+)?(?:буду|окажусь|выйду|приду)\s+(?:около|возле|у|рядом\s+с|в|на)\s+(.+?)\.?$)"};

    const auto geo = kGeo.match(t);
    if (!geo) {
        return std::nullopt;
    }
    return PlaceReminderCommand{tidy(geo->groups[1]), tidy(geo->groups[2])};
}

/// @brief Напоминание по времени: «напомни завтра в 15:00 позвонить маме».
[[nodiscard]] std::optional<TimedReminderCommand> parseTimedReminder(const std::string& t,
                                                                     LocalSeconds now)
{
    static const Pcre2Regex kRemind{R"(^напомни(?:ть)?\s+(.+)$)"};

    const auto rem = kRemind.match(t);
    if (!rem) {
        return std::nullopt;
    }
    When w = extractWhen(rem->groups[1], now);
    const auto today = dayOf(now);
    const auto nowMinutes = std::chrono::floor<std::chrono::minutes>(now);

    LocalMinutes dueAt{};
    if (w.time) {
        const auto day = w.date.value_or(today);
        dueAt = std::chrono::local_days{std::chrono::sys_days{day}.time_since_epoch()} + *w.time;
        // Время уже прошло сегодня — переносим на завтра.
        if (!w.date && dueAt <= nowMinutes) {
            dueAt += std::chrono::days{kTomorrowOffset};
        }
    } else if (w.date) {
        // Дата без времени — утро этого дня.
        dueAt = std::chrono::local_days{std::chrono::sys_days{*w.date}.time_since_epoch()}
                + std::chrono::minutes{kMorningHour * kMinutesPerHour};
    } else {
        // Ничего не указано — через час от текущего момента.
        dueAt = nowMinutes + std::chrono::hours{1};
    }
    return TimedReminderCommand{tidy(w.cleaned), dueAt};
}

/// @brief Убирает кавычки по краям заголовка.
[[nodiscard]] std::string stripQuotes(std::string_view title)
{
    static const Pcre2Regex kQuotes{R"(^[«"']+|[»"']+$)"};
    return replaceAll(title, kQuotes, "");
}

/// @brief Задача: «добавь задачу купить продукты на завтра».
[[nodiscard]] std::optional<TaskCommand> parseTaskCommand(const std::string& t, LocalSeconds now)
{
    static const Pcre2Regex kTaskRe{R"(^(?:добавь|создай|запиши)\s+задачу:?\s+(.+)$)"};

    const auto task = kTaskRe.match(t);
    if (!task) {
        return std::nullopt;
    }
    When w = extractWhen(task->groups[1], now);
    return TaskCommand{tidy(w.cleaned), w.date};
}

/// @brief Событие в свободной форме: «завтра тренировка в 18:00, добавь в календарь».
[[nodiscard]] std::optional<EventCommand> parseFreeEvent(const std::string& t, LocalSeconds now)
{
    static const Pcre2Regex kAddWord{R"(добавь|создай|запиши)"};
    static const Pcre2Regex kEventWord{R"(календар|событ|встреч|мероприят)"};
    static const Pcre2Regex kAnchored{
        R"(^(?:добавь|создай|запиши)\s+(?:в\s+календарь|встречу|событие|мероприятие))"};
    static const Pcre2Regex kTail{
        R"([,.]?\s*(?:добавь|создай|запиши)(?:те)?(?:\s+(?:это|её|его))?(?:\s+пожалуйста)?(?:\s+(?:в\s+календарь|событие|как\s+событие))?[.!]?\s*$)"};
    static const Pcre2Regex kHead{
        R"(^(?:добавь|создай|запиши)(?:те)?\s+(?:пожалуйста\s+)?(?:в\s+календарь[\s:]*)?(?:событие|встречу|мероприятие|запись)?[\s:]*)"};
    static const Pcre2Regex kIntro{
        R"(^(?:у\s+меня\s+(?:будет\s+)?|будет\s+|запланируй\s+))"};

    if (!kAddWord.match(t) || !kEventWord.match(t) || kAnchored.match(t)) {
        return std::nullopt;
    }
    std::string s = replaceAll(t, kTail, "");
    s = replaceAll(s, kHead, "");

    When w = extractWhen(s, now);
    std::string title = replaceAll(trimmedText(w.cleaned), kIntro, "");
    title = stripQuotes(tidy(title));
    if (utf8::charCount(title) < kMinTitleLength) {
        return std::nullopt;
    }
    return EventCommand{title, w.date.value_or(dayOf(now)), w.time};
}

/// @brief Задача в свободной форме: «…прочитать книгу, добавь в задачи».
[[nodiscard]] std::optional<TaskCommand> parseFreeTask(const std::string& t, LocalSeconds now)
{
    static const Pcre2Regex kAddWord{R"(добавь)"};
    static const Pcre2Regex kTaskWord{R"(задач)"};
    static const Pcre2Regex kTail{
        R"([,.]?\s*добавь(?:те)?(?:\s+(?:это|её|его))?(?:\s+пожалуйста)?(?:\s+в\s+(?:задачи|список\s+задач))?[.!]?\s*$)"};
    static const Pcre2Regex kHead{
        R"(^добавь(?:те)?\s+(?:пожалуйста\s+)?в\s+(?:задачи|список\s+задач):?\s*)"};
    static const Pcre2Regex kIntro{
        R"(^(?:у\s+меня\s+(?:появилась|есть)\s+(?:новая\s+)?задача[\s:—-]+|сегодня\s+(?:мне\s+)?(?:надо|нужно)\s+|мне\s+(?:надо|нужно)\s+|надо\s+|нужно\s+))"};

    if (!kAddWord.match(t) || !kTaskWord.match(t)) {
        return std::nullopt;
    }
    std::string s = replaceAll(t, kTail, "");
    s = replaceAll(s, kHead, "");

    // «сегодня» до удаления — учитываем как срок
    When w = extractWhen(s, now);
    std::string title = replaceAll(trimmedText(w.cleaned), kIntro, "");
    title = stripQuotes(trimmedText(title));
    title = tidy(title);
    if (utf8::charCount(title) < kMinTitleLength) {
        return std::nullopt;
    }
    return TaskCommand{title, w.date};
}

/// @brief Событие с командой в начале: «добавь встречу с Иваном в пятницу в 18:00».
[[nodiscard]] std::optional<EventCommand> parseAnchoredEvent(const std::string& t, LocalSeconds now)
{
    static const Pcre2Regex kHint{
        R"(^(?:добавь|создай|запиши)\s+(?:в\s+календарь|встречу|событие|мероприятие))"};
    static const Pcre2Regex kEventRe{
        R"(^(?:добавь|создай|запиши)\s+(?:в\s+календарь\s+)?(встречу|событие|мероприятие|запись)?\s*(.+)$)"};

    if (!kHint.match(t)) {
        return std::nullopt;
    }
    const auto ev = kEventRe.match(t);
    if (!ev) {
        return std::nullopt;
    }
    When w = extractWhen(ev->groups[2], now);
    std::string title = tidy(w.cleaned);
    if (utf8::charCount(title) < kMinTitleLength) {
        return std::nullopt;
    }
    // «встречу с Иваном» → «Встреча с Иваном»
    if (utf8::toLower(ev->groups[1]) == "встречу" && !title.empty()) {
        title = "Встреча " + utf8::lowerFirst(title);
    }
    return EventCommand{title, w.date.value_or(dayOf(now)), w.time};
}

[[nodiscard]] Command parseCommand(std::string_view text, LocalSeconds now)
{
    const std::string t = stripLeadingPhrases(text);

    static const Pcre2Regex kRemember{R"(^запомни(?:те)?[:,]?\s+(?:что\s+)?(.+)$)"};
    if (const auto rem = kRemember.match(t)) {
        std::string fact = trimmedText(rem->groups[1]);
        while (fact.ends_with('.') || fact.ends_with('!')) {
            fact.pop_back();
        }
        return RememberCommand{fact};
    }

    if (auto recurring = parseRecurring(t)) {
        return std::move(*recurring);
    }
    if (auto list = parseTaskList(t)) {
        return std::move(*list);
    }
    if (auto route = parseRoute(t)) {
        return std::move(*route);
    }
    if (auto place = parsePlaceReminder(t)) {
        return std::move(*place);
    }
    if (auto timed = parseTimedReminder(t, now)) {
        return std::move(*timed);
    }
    if (auto task = parseTaskCommand(t, now)) {
        return std::move(*task);
    }
    if (auto event = parseFreeEvent(t, now)) {
        return std::move(*event);
    }
    if (auto task = parseFreeTask(t, now)) {
        return std::move(*task);
    }
    if (auto event = parseAnchoredEvent(t, now)) {
        return std::move(*event);
    }
    return NoCommand{};
}

}  // namespace

Command CommandParser::parse(std::string_view text, LocalSeconds now) const
{
    Command command = parseCommand(text, now);
    const bool valid = std::visit([](const auto& value) {
        if constexpr (requires { value.title; }) {
            return utf8::charCount(value.title) >= kMinTitleLength;
        } else if constexpr (requires { value.fact; }) {
            return utf8::charCount(value.fact) >= kMinTitleLength;
        } else if constexpr (requires { value.place; }) {
            return utf8::charCount(value.place) >= kMinTitleLength;
        } else {
            return true;
        }
    }, command);
    if (!valid) {
        return NoCommand{};
    }

    return command;
}

}  // namespace msa::core
