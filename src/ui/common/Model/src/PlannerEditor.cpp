#include "msa/ui/PlannerEditor.h"

#include <QDate>
#include <QTime>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace msa::ui
{
namespace
{
constexpr int kMinutesPerHour = 60;
constexpr int kLocalMomentLength = 19;
constexpr int kDateLength = 10;

std::int64_t identifier(const QString& text)
{
    bool valid{false};
    const auto number = text.toLongLong(&valid);
    if (!valid || number <= 0 || QString::number(number) != text)
    {
        throw std::invalid_argument{"Неверный ID записи."};
    }
    return number;
}
std::chrono::year_month_day date(const QString& text)
{
    const auto parsed = QDate::fromString(text, Qt::ISODate);
    if (!parsed.isValid() || parsed.year() < 1 || parsed.year() > 9999 || parsed.toString(Qt::ISODate) != text)
    {
        throw std::invalid_argument{"Введите дату в формате ГГГГ-ММ-ДД."};
    }
    return std::chrono::year{parsed.year()} / static_cast<unsigned>(parsed.month()) /
           static_cast<unsigned>(parsed.day());
}
std::chrono::minutes time(const QString& text)
{
    const auto parsed = QTime::fromString(text, "HH:mm");
    if (!parsed.isValid() || parsed.toString("HH:mm") != text)
    {
        throw std::invalid_argument{"Введите время в формате ЧЧ:ММ."};
    }
    return std::chrono::minutes{parsed.hour() * kMinutesPerHour + parsed.minute()};
}
std::chrono::local_seconds moment(const QString& text)
{
    if (text.size() != kLocalMomentLength || text.at(kDateLength) != 'T')
    {
        throw std::invalid_argument{"Введите местный момент в формате ГГГГ-ММ-ДДTЧЧ:ММ:СС."};
    }
    const auto day = date(text.first(kDateLength));
    const auto parsed = QTime::fromString(text.sliced(kDateLength + 1), "HH:mm:ss");
    if (!parsed.isValid() || parsed.toString("HH:mm:ss") != text.sliced(kDateLength + 1))
    {
        throw std::invalid_argument{"Укажите корректное время напоминания."};
    }
    return std::chrono::local_days{std::chrono::sys_days{day}.time_since_epoch()} + std::chrono::hours{parsed.hour()} +
           std::chrono::minutes{parsed.minute()} + std::chrono::seconds{parsed.second()};
}
std::string requiredText(const QString& text)
{
    if (text.trimmed().isEmpty())
    {
        throw std::invalid_argument{"Заполните обязательные текстовые поля."};
    }
    return text.toStdString();
}
double positiveNumber(QString text)
{
    text.replace(',', '.');
    bool valid{false};
    const auto number = text.toDouble(&valid);
    if (!valid || !std::isfinite(number) || number <= 0)
    {
        throw std::invalid_argument{"Количество должно быть положительным конечным числом."};
    }
    return number;
}
int choice(const QString& text, const QStringList& values)
{
    const auto index = values.indexOf(text);
    if (index < 0)
    {
        throw std::invalid_argument{"Выберите допустимый вариант."};
    }
    return static_cast<int>(index);
}
} // namespace

PlannerEditor::PlannerEditor(ClientModels& models, QObject* parent) : QObject{parent}, m_models{models}
{
    connect(&models, &ClientModels::connectionChanged, this,
            [this]()
            {
                if (m_active && !m_requiresReopen &&
                    (m_session != m_models.sessionRevision() || (m_pending && !m_models.online())))
                {
                    m_requiresReopen = true;
                    m_error = m_pending
                                  ? "Связь прервалась. Итог записи неизвестен. Обновите данные и откройте форму заново."
                                  : "Сессия изменилась. После обновления данных откройте форму заново.";
                    m_pending.reset();
                }
                emit statusChanged();
            });
    connect(&models, &ClientModels::plannerActionFinished, this,
            [this](const core::ClientPlannerAction& action, bool success, const QString& message, const QString& effect)
            {
                if (!m_pending || *m_pending != action)
                {
                    return;
                }
                m_pending.reset();
                if (success)
                {
                    m_active = false;
                    emit formChanged();
                }
                else
                {
                    m_requiresReopen = effect == "unknown";
                    m_error =
                        m_requiresReopen ? "Итог записи неизвестен. Обновите данные и откройте форму заново." : message;
                }
                emit statusChanged();
            });
}
bool PlannerEditor::active() const
{
    return m_active;
}
QString PlannerEditor::heading() const
{
    return m_heading;
}
QString PlannerEditor::explanation() const
{
    return m_explanation;
}
bool PlannerEditor::pending() const
{
    return m_pending.has_value();
}
bool PlannerEditor::editable() const
{
    return !pending() && !m_requiresReopen;
}
bool PlannerEditor::canSubmit() const
{
    return m_active && editable() && m_models.online();
}
QString PlannerEditor::errorText() const
{
    return m_error;
}
QString PlannerEditor::value(const QString& key) const
{
    const auto found = m_values.find(key);
    return found == m_values.end() ? QString{} : found->second;
}
bool PlannerEditor::destructive() const
{
    return m_kind == FormKind::DeleteTask || m_kind == FormKind::DeleteEvent || m_kind == FormKind::DeleteHabit ||
           m_kind == FormKind::DeleteReminder;
}
QVariantList PlannerEditor::fields() const
{
    QVariantList result;
    for (const auto& field : m_fields)
    {
        result.push_back(QVariantMap{{"key", field.key},
                                     {"label", field.label},
                                     {"value", value(field.key)},
                                     {"options", field.labels},
                                     {"optionIndex", field.values.indexOf(value(field.key))},
                                     {"multiline", field.multiline}});
    }
    return result;
}
QVariantMap PlannerEditor::fieldVisibility() const
{
    QVariantMap result;
    for (const auto& field : m_fields)
    {
        const auto hidden =
            m_kind == FormKind::NewReminder && ((field.key == "dueAt" && value("kind") != "Timed") ||
                                                (field.key == "interval" && value("kind") != "Recurring") ||
                                                (field.key == "place" && value("kind") != "Place"));
        result.insert(field.key, !hidden);
    }
    return result;
}
void PlannerEditor::setError(const QString& message)
{
    m_error = message;
    emit statusChanged();
}
void PlannerEditor::addField(const QString& key, const QString& label, const QString& initial,
                             const QStringList& values, const QStringList& labels, bool multiline)
{
    m_fields.push_back({key, label, values, labels, multiline});
    m_values[key] = initial;
}
QVariantMap PlannerEditor::source(FormKind kind, const QString& identifier) const
{
    switch (kind)
    {
    case FormKind::EditTask:
    case FormKind::DeleteTask:
        return m_models.tasks()->rowByIdentifier(identifier);
    case FormKind::EditEvent:
    case FormKind::DeleteEvent:
    {
        const auto today = m_models.eventsToday()->rowByIdentifier(identifier);
        return today.isEmpty() ? m_models.calendarEvents()->rowByIdentifier(identifier) : today;
    }
    case FormKind::LogHabit:
    case FormKind::DeleteHabit:
        return m_models.habits()->rowByIdentifier(identifier);
    case FormKind::DeleteReminder:
        return m_models.reminders()->rowByIdentifier(identifier);
    default:
        return {};
    }
}
void PlannerEditor::open(FormKind kind, const QString& identifier)
{
    if (pending())
    {
        return;
    }
    const auto row = source(kind, identifier);
    if (!identifier.isEmpty() && row.isEmpty())
    {
        m_models.setError("Запись не найдена. Обновите данные.");
        return;
    }
    m_kind = kind;
    m_identifier = identifier;
    m_session = m_models.sessionRevision();
    m_fields.clear();
    m_values.clear();
    m_error.clear();
    m_explanation.clear();
    m_requiresReopen = false;
    m_active = true;
    if (kind == FormKind::NewTask || kind == FormKind::EditTask)
    {
        m_heading = identifier.isEmpty() ? "Новая задача" : "Изменить задачу";
        addField("title", "Название", row.value("title").toString());
        addField("dueDate", "Срок (ГГГГ-ММ-ДД, можно оставить пустым)", row.value("dueDate").toString());
        addField("priority", "Приоритет", row.value("priority", "Normal").toString(), {"Low", "Normal", "High"},
                 {"Низкий", "Обычный", "Высокий"});
    }
    else if (kind == FormKind::NewEvent || kind == FormKind::EditEvent)
    {
        m_heading = identifier.isEmpty() ? "Новое событие" : "Изменить событие";
        m_explanation = "Дата и время относятся к зоне сервера: " + m_models.timeZone() +
                        ". Изменение относится ко всей исходной серии.";
        addField("title", "Название", row.value("title").toString());
        addField("date", "Исходная дата (ГГГГ-ММ-ДД)", row.value("date").toString());
        addField("time", "Время (ЧЧ:ММ, можно оставить пустым)", row.value("time").toString());
        addField("description", "Описание", row.value("description").toString(), {}, {}, true);
        addField("repeat", "Повтор", row.value("repeat", "None").toString(), {"None", "Daily", "Weekly", "Monthly"},
                 {"Не повторять", "Ежедневно", "Еженедельно", "Ежемесячно"});
    }
    else if (kind == FormKind::NewHabit)
    {
        m_heading = "Новая привычка";
        addField("name", "Название");
        addField("target", "Цель", "1");
        addField("unit", "Единица измерения (необязательно)");
        addField("period", "Период", "Day", {"Day", "Week"}, {"День", "Неделя"});
    }
    else if (kind == FormKind::LogHabit)
    {
        m_heading = "Отметить привычку";
        m_explanation = row.value("name").toString() + ". Отметка сохраняется за сегодняшний день сервера.";
        addField("value", "Добавить (" + row.value("unit").toString() + ")", "1");
    }
    else if (kind == FormKind::NewReminder)
    {
        m_heading = "Новое напоминание";
        m_explanation = "Время — в зоне сервера: " + m_models.timeZone() +
                        ". Для повтора первый срок определит сервер. Место сохраняется без координат.";
        addField("title", "Текст");
        addField("kind", "Вид", "Timed", {"Timed", "Recurring", "Place"}, {"По времени", "Повторяющееся", "По месту"});
        addField("dueAt", "Местный момент (ГГГГ-ММ-ДДTЧЧ:ММ:СС)");
        addField("interval", "Интервал в минутах", "60");
        addField("place", "Место или адрес");
    }
    else
    {
        m_heading = "Подтвердите удаление";
        m_explanation = "Удалить «" + row.value("title", row.value("name")).toString() + "»?";
        if (kind == FormKind::DeleteEvent)
        {
            m_explanation += " Будет удалена вся серия событий.";
        }
        if (kind == FormKind::DeleteHabit)
        {
            m_explanation += " Будет удалён и журнал привычки.";
        }
    }
    emit formChanged();
    emit statusChanged();
}
void PlannerEditor::newTask()
{
    open(FormKind::NewTask);
}
void PlannerEditor::editTask(const QString& identifier)
{
    open(FormKind::EditTask, identifier);
}
void PlannerEditor::deleteTask(const QString& identifier)
{
    open(FormKind::DeleteTask, identifier);
}
void PlannerEditor::newEvent()
{
    open(FormKind::NewEvent);
}
void PlannerEditor::editEvent(const QString& identifier)
{
    open(FormKind::EditEvent, identifier);
}
void PlannerEditor::deleteEvent(const QString& identifier)
{
    open(FormKind::DeleteEvent, identifier);
}
void PlannerEditor::newHabit()
{
    open(FormKind::NewHabit);
}
void PlannerEditor::recordHabit(const QString& identifier)
{
    open(FormKind::LogHabit, identifier);
}
void PlannerEditor::deleteHabit(const QString& identifier)
{
    open(FormKind::DeleteHabit, identifier);
}
void PlannerEditor::newReminder()
{
    open(FormKind::NewReminder);
}
void PlannerEditor::deleteReminder(const QString& identifier)
{
    open(FormKind::DeleteReminder, identifier);
}
void PlannerEditor::setField(const QString& key, const QString& text)
{
    if (!editable() || !m_values.contains(key))
    {
        return;
    }
    const auto field = std::ranges::find_if(m_fields, [&key](const Field& field) { return field.key == key; });
    if (field == m_fields.end() || !field->values.isEmpty())
    {
        return;
    }
    m_values[key] = text;
    setError({});
}
void PlannerEditor::setChoice(const QString& key, int index)
{
    if (!editable())
    {
        return;
    }
    const auto field = std::ranges::find_if(m_fields, [&key](const Field& field) { return field.key == key; });
    if (field == m_fields.end() || index < 0 || index >= field->values.size())
    {
        return;
    }
    m_values[key] = field->values.at(index);
    setError({});
}
core::ClientPlannerAction PlannerEditor::taskAction() const
{
    return core::ClientSaveTask{m_identifier.isEmpty() ? std::nullopt : std::optional{identifier(m_identifier)},
                                requiredText(value("title")),
                                value("dueDate").isEmpty() ? std::nullopt : std::optional{date(value("dueDate"))},
                                static_cast<core::TaskPriority>(choice(value("priority"), {"Low", "Normal", "High"}))};
}
core::ClientPlannerAction PlannerEditor::eventAction() const
{
    return core::ClientSaveEvent{
        m_identifier.isEmpty() ? std::nullopt : std::optional{identifier(m_identifier)},
        requiredText(value("title")),
        date(value("date")),
        value("time").isEmpty() ? std::nullopt : std::optional{time(value("time"))},
        value("description").toStdString(),
        static_cast<core::EventRepeat>(choice(value("repeat"), {"None", "Daily", "Weekly", "Monthly"}))};
}
core::ClientPlannerAction PlannerEditor::reminderAction() const
{
    const auto title = requiredText(value("title"));
    if (value("kind") == "Timed")
    {
        return core::ClientCreateTimedReminder{title, moment(value("dueAt"))};
    }
    if (value("kind") == "Place")
    {
        return core::ClientCreatePlaceReminder{title, requiredText(value("place"))};
    }
    bool valid{false};
    const auto interval = value("interval").toLongLong(&valid);
    if (!valid || interval <= 0)
    {
        throw std::invalid_argument{"Интервал должен быть положительным целым числом минут."};
    }
    return core::ClientCreateRecurringReminder{title, std::chrono::minutes{interval}};
}
core::ClientPlannerAction PlannerEditor::buildAction() const
{
    if (m_kind == FormKind::NewTask || m_kind == FormKind::EditTask)
    {
        return taskAction();
    }
    if (m_kind == FormKind::NewEvent || m_kind == FormKind::EditEvent)
    {
        return eventAction();
    }
    if (m_kind == FormKind::NewReminder)
    {
        return reminderAction();
    }
    if (m_kind == FormKind::NewHabit)
    {
        return core::ClientCreateHabit{requiredText(value("name")), positiveNumber(value("target")),
                                       value("unit").toStdString(),
                                       static_cast<core::HabitPeriod>(choice(value("period"), {"Day", "Week"}))};
    }
    if (m_kind == FormKind::LogHabit)
    {
        return core::ClientLogHabit{identifier(m_identifier), positiveNumber(value("value"))};
    }
    const auto kind = m_kind == FormKind::DeleteTask    ? core::ClientItemKind::Task
                      : m_kind == FormKind::DeleteEvent ? core::ClientItemKind::Event
                      : m_kind == FormKind::DeleteHabit ? core::ClientItemKind::Habit
                                                        : core::ClientItemKind::Reminder;
    return core::ClientDeleteItem{kind, identifier(m_identifier)};
}
bool PlannerEditor::submit()
{
    if (!canSubmit())
    {
        return false;
    }
    if (!m_identifier.isEmpty() && source(m_kind, m_identifier).isEmpty())
    {
        setError("Запись больше недоступна. Обновите данные и откройте форму заново.");
        return false;
    }
    try
    {
        m_pending = buildAction();
        emit statusChanged();
        const auto action = *m_pending;
        if (!m_models.submitPlannerAction(action))
        {
            m_pending.reset();
            emit statusChanged();
            return false;
        }
        return true;
    }
    catch (const std::exception& error)
    {
        setError(QString::fromUtf8(error.what()));
        return false;
    }
}
void PlannerEditor::cancel()
{
    if (pending())
    {
        return;
    }
    m_active = false;
    emit formChanged();
}
} // namespace msa::ui
