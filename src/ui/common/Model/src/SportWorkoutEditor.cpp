#include "msa/ui/SportWorkoutEditor.h"

#include <QDate>
#include <QStringList>
#include <algorithm>
#include <stdexcept>

namespace msa::ui
{
namespace
{
[[nodiscard]] QString dateText(std::chrono::year_month_day value)
{
    return QDate{static_cast<int>(value.year()), static_cast<int>(static_cast<unsigned>(value.month())),
                 static_cast<int>(static_cast<unsigned>(value.day()))}
        .toString(Qt::ISODate);
}
[[nodiscard]] QString duration(core::SportDuration value)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(value).count();
    return QString{"%1:%2:%3"}
        .arg(seconds / 3600)
        .arg((seconds / 60) % 60, 2, 10, QChar{'0'})
        .arg(seconds % 60, 2, 10, QChar{'0'});
}
[[nodiscard]] QString number(std::optional<int> value)
{
    return value ? QString::number(*value) : QString{};
}
[[nodiscard]] QStringList areas(const sportForm::Text& text, const std::string& prefix,
                                const core::SportWellbeing& fallback)
{
    QStringList result;
    for (const auto& area : fallback.areas)
    {
        result.append(QString::fromStdString(area));
    }
    return sportForm::text(text, prefix + "/areas", result.join(';')).split(';', Qt::SkipEmptyParts);
}
[[nodiscard]] core::SportWellbeing readWellbeing(const core::SportWellbeing& fallback, const sportForm::Text& text,
                                                 const std::string& prefix)
{
    auto result = fallback;
    const auto rating = [&text, prefix](const std::string& key, std::optional<int> previous, int minimum,
                                        int maximum) -> std::optional<int>
    {
        const auto value = sportForm::text(text, prefix + "/" + key, number(previous));
        return value.isEmpty() ? std::nullopt
                               : std::optional{sportForm::integer(value, minimum, maximum, "Оценка состояния")};
    };
    result.wellbeing = rating("wellbeing", result.wellbeing, 1, 5);
    result.fatigue = rating("fatigue", result.fatigue, 1, 5);
    result.pain = rating("pain", result.pain, 0, 10);
    result.areas.clear();
    for (const auto& area : areas(text, prefix, fallback))
    {
        result.areas.push_back(area.toStdString());
    }
    result.otherArea =
        sportForm::text(text, prefix + "/other", QString::fromStdString(fallback.otherArea)).toStdString();
    result.comment = sportForm::text(text, prefix + "/comment", QString::fromStdString(fallback.comment)).toStdString();
    const auto newPain =
        sportForm::text(text, prefix + "/newPain", fallback.newPain ? (*fallback.newPain ? "yes" : "no") : "");
    if (newPain != "yes" && newPain != "no" && !newPain.isEmpty())
    {
        throw std::runtime_error{"Выберите отметку новой боли."};
    }
    result.newPain = newPain.isEmpty() ? std::nullopt : std::optional{newPain == "yes"};
    if (const auto error = core::validateSportWellbeing(result))
    {
        throw std::runtime_error{error->reason};
    }
    return result;
}
[[nodiscard]] core::SportActualSet readActual(const core::SportActualSet& actual, const sportForm::Text& text)
{
    core::SportPlannedSet base{actual.identifier, actual.repetitions.value_or(0), actual.load, actual.side};
    const auto parsed = sportForm::readSet(base, text);
    auto result = actual;
    result.repetitions = parsed.repetitions;
    result.load = parsed.load;
    result.side = parsed.side;
    result.state = core::SportSetState::Performed;
    result.skipReason.clear();
    return result;
}
[[nodiscard]] QString state(core::SportSetState value)
{
    switch (value)
    {
    case core::SportSetState::Pending:
        return "Не отмечен";
    case core::SportSetState::Performed:
        return "Выполнен";
    case core::SportSetState::Skipped:
        return "Пропущен";
    }
    return {};
}
} // namespace
SportWorkoutEditor::SportWorkoutEditor(QObject* parent) : QObject{parent} {}
void SportWorkoutEditor::openDraft(core::SportCheckpoint checkpoint)
{
    m_error.clear();
    m_liveElapsed = {};
    m_checkpoint = std::move(checkpoint);
    m_original.reset();
    m_correcting = false;
    m_visible = true;
    m_saved = m_checkpoint.sequence > 0;
    m_discardRequested = false;
    m_status = m_saved ? "Черновик сохранён на устройстве" : "Черновик ещё не сохранён";
    emit changed();
    emit timeChanged();
    emit statusChanged();
}
void SportWorkoutEditor::openResult(core::SportWorkoutRevision revision)
{
    m_error.clear();
    m_liveElapsed = {};
    m_checkpoint = {revision.workout, {}, 0};
    m_original = std::move(revision);
    m_correcting = false;
    m_visible = true;
    m_saved = true;
    m_discardRequested = false;
    m_status = "Сохранено на устройстве · синхронизация ещё не подключена";
    emit changed();
    emit timeChanged();
    emit statusChanged();
}
bool SportWorkoutEditor::visible() const
{
    return m_visible;
}
bool SportWorkoutEditor::active() const
{
    return !m_checkpoint.draft.identifier.empty() && !m_original;
}
bool SportWorkoutEditor::editing() const
{
    return active() || m_correcting;
}
bool SportWorkoutEditor::correcting() const
{
    return m_correcting;
}
bool SportWorkoutEditor::discardRequested() const
{
    return m_discardRequested;
}
bool SportWorkoutEditor::saved() const
{
    return m_saved;
}
QString SportWorkoutEditor::restrictionsSnapshot() const
{
    return QString::fromStdString(m_checkpoint.draft.restrictionsSnapshot);
}
QString SportWorkoutEditor::title() const
{
    return QString::fromStdString(m_checkpoint.draft.originalPlan.title);
}
QString SportWorkoutEditor::status() const
{
    return m_error.isEmpty() ? m_status : m_error + " · " + m_status;
}
const core::SportCheckpoint& SportWorkoutEditor::checkpoint() const
{
    return m_checkpoint;
}
const std::optional<core::SportWorkoutRevision>& SportWorkoutEditor::original() const
{
    return m_original;
}
QVariantList SportWorkoutEditor::fields() const
{
    const auto& draft = m_checkpoint.draft;
    return {sportForm::field("date", "Дата занятия (ГГГГ-ММ-ДД)",
                             sportForm::text(m_checkpoint.editorText, "date", dateText(draft.date))),
            sportForm::field(
                "duration", "Итоговая длительность, с (необязательно, 1–604800)",
                sportForm::text(
                    m_checkpoint.editorText, "duration",
                    draft.timer.manualDuration
                        ? QString::number(
                              std::chrono::duration_cast<std::chrono::seconds>(*draft.timer.manualDuration).count())
                        : QString{})),
            sportForm::field("notes", "Заметки о занятии",
                             sportForm::text(m_checkpoint.editorText, "notes", QString::fromStdString(draft.notes)))};
}
QVariantList SportWorkoutEditor::items() const
{
    QVariantList result;
    for (const auto& item : m_checkpoint.draft.items)
    {
        QVariantList sets;
        for (const auto& actual : item.actual)
        {
            const auto plan =
                std::ranges::find(item.plan.sets, actual.plannedIdentifier, &core::SportPlannedSet::identifier);
            const auto planned = plan == item.plan.sets.end()
                                     ? QString{"Вне плана"}
                                     : QString{"План: %1 повт. · %2 · %3"}
                                           .arg(plan->repetitions)
                                           .arg(sportForm::load(plan->load), sportForm::side(plan->side));
            const auto fact = actual.state == core::SportSetState::Performed
                                  ? QString{"%1 повт. · %2 · %3"}
                                        .arg(*actual.repetitions)
                                        .arg(sportForm::load(actual.load), sportForm::side(actual.side))
                                  : state(actual.state);
            auto fields = sportForm::setFields(actual.identifier, actual.load, m_checkpoint.editorText,
                                               number(actual.repetitions));
            if (item.plan.exercise.sides.size() > 1)
            {
                fields.append(sportForm::sideField(actual.identifier, actual.side, item.plan.exercise.sides,
                                                   m_checkpoint.editorText));
            }
            bool extraLoadPresent{};
            for (const auto& value : fields)
            {
                const auto field = value.toMap();
                if (field.value("optionalLoad").toBool() && !field.value("value").toString().isEmpty())
                {
                    extraLoadPresent = true;
                }
            }
            sets.append(QVariantMap{{"identifier", QString::fromStdString(actual.identifier)},
                                    {"state", state(actual.state)},
                                    {"bodyweight", actual.load.index() == 0},
                                    {"extraLoadPresent", extraLoadPresent},
                                    {"plan", planned},
                                    {"fact", fact},
                                    {"fields", fields}});
        }
        result.append(QVariantMap{
            {"identifier", QString::fromStdString(item.plan.identifier)},
            {"exerciseId", QString::fromStdString(item.plan.exercise.identifier)},
            {"title", QString::fromStdString(item.plan.exercise.title)},
            {"brief", QString::fromStdString(item.plan.exercise.technique.brief)},
            {"replacement", !item.replacesItem.empty()},
            {"replacementFields",
             QVariantList{
                 sportForm::field(item.plan.identifier + "/replacementSets", "План замены: количество подходов (1–100)",
                                  sportForm::text(m_checkpoint.editorText, item.plan.identifier + "/replacementSets")),
                 sportForm::field(
                     item.plan.identifier + "/replacementReps", "План замены: повторения (1–1000)",
                     sportForm::text(m_checkpoint.editorText, item.plan.identifier + "/replacementReps"))}},
            {"conditions", sportForm::conditionFields(item.plan, m_checkpoint.editorText)},
            {"sets", sets}});
    }
    return result;
}
QVariantList SportWorkoutEditor::wellbeing() const
{
    QVariantList result;
    for (const auto& prefix : {std::string{"before"}, std::string{"after"}})
    {
        const auto& value = prefix == "before" ? m_checkpoint.draft.before : m_checkpoint.draft.after;
        QVariantList fields{
            sportForm::field(prefix + "/wellbeing", "Самочувствие, 1–5 (необязательно)",
                             sportForm::text(m_checkpoint.editorText, prefix + "/wellbeing", number(value.wellbeing))),
            sportForm::field(prefix + "/fatigue", "Усталость, 1–5 (необязательно)",
                             sportForm::text(m_checkpoint.editorText, prefix + "/fatigue", number(value.fatigue))),
            sportForm::field(prefix + "/pain", "Боль, 0–10 (необязательно)",
                             sportForm::text(m_checkpoint.editorText, prefix + "/pain", number(value.pain))),
            sportForm::field(
                prefix + "/other", "Другая область: уточнение",
                sportForm::text(m_checkpoint.editorText, prefix + "/other", QString::fromStdString(value.otherArea))),
            sportForm::field(
                prefix + "/comment", "Комментарий о состоянии",
                sportForm::text(m_checkpoint.editorText, prefix + "/comment", QString::fromStdString(value.comment)))};
        const auto selected = areas(m_checkpoint.editorText, prefix, value);
        QVariantList choices;
        QStringList selectedLabels;
        const std::vector<std::pair<QString, QString>> bodyAreas{{"neck", "Шея"},
                                                                 {"shoulder", "Плечо"},
                                                                 {"back", "Спина"},
                                                                 {"elbow", "Локоть"},
                                                                 {"wrist", "Кисть"},
                                                                 {"hip", "Таз / бедро"},
                                                                 {"knee", "Колено"},
                                                                 {"ankle", "Голеностоп / стопа"},
                                                                 {"other", "Другая область"}};
        for (const auto& [key, label] : bodyAreas)
        {
            choices.append(QVariantMap{{"key", key}, {"title", label}, {"selected", selected.contains(key)}});
            if (selected.contains(key))
            {
                selectedLabels.append(label);
            }
        }
        const auto newPain = sportForm::text(m_checkpoint.editorText, prefix + "/newPain",
                                             value.newPain ? (*value.newPain ? "yes" : "no") : "");
        result.append(
            QVariantMap{{"prefix", QString::fromStdString(prefix)},
                        {"title", prefix == "before" ? "До занятия" : "После занятия"},
                        {"fields", fields},
                        {"areas", choices},
                        {"newPain", newPain},
                        {"areaSummary", selectedLabels.isEmpty() ? QString{"Не указаны"} : selectedLabels.join(", ")},
                        {"newPainSummary", newPain.isEmpty() ? "Не указано" : (newPain == "yes" ? "Да" : "Нет")}});
    }
    return result;
}
QVariantList SportWorkoutEditor::absences() const
{
    QVariantList result;
    for (const auto& interval : m_checkpoint.draft.timer.absences)
    {
        const auto key = interval.identifier + "/activeSeconds";
        const auto active =
            interval.active
                ? QString::number(std::chrono::duration_cast<std::chrono::seconds>(*interval.active).count())
                : QString{};
        result.append(QVariantMap{{"identifier", QString::fromStdString(interval.identifier)},
                                  {"resolved", interval.decision.has_value()},
                                  {"title", interval.duration ? "Время отсутствия: " + duration(*interval.duration)
                                                              : "Длительность отсутствия недостоверна: проверьте часы"},
                                  {"summary", interval.active ? "Учтено активностью: " + duration(*interval.active)
                                                              : "Выберите, как учесть интервал"},
                                  {"field", sportForm::field(key, "Активная часть этого интервала, с (можно 0)",
                                                             sportForm::text(m_checkpoint.editorText, key, active))}});
    }
    return result;
}
QVariantMap SportWorkoutEditor::timeSummary() const
{
    auto summary = core::summarizeSportDuration(m_checkpoint.draft.timer);
    if (summary.total)
    {
        *summary.total += m_liveElapsed;
    }
    if (!m_checkpoint.draft.timer.paused)
    {
        summary.active += m_liveElapsed;
    }
    return {{"total", summary.total ? duration(*summary.total) : "Не определено"},
            {"active", duration(summary.active)},
            {"corrected", summary.corrected ? duration(*summary.corrected) : "Не задана"},
            {"paused", m_checkpoint.draft.timer.paused},
            {"unresolved", summary.unresolved},
            {"warning", summary.warning}};
}
core::SportResult<core::SportWorkoutDraft> SportWorkoutEditor::candidate() const
{
    try
    {
        auto draft = m_checkpoint.draft;
        const auto date =
            QDate::fromString(sportForm::text(m_checkpoint.editorText, "date", dateText(draft.date)), Qt::ISODate);
        if (!date.isValid() || date.year() < 1 || date.year() > 9999)
        {
            throw std::runtime_error{"Проверьте дату: ГГГГ-ММ-ДД."};
        }
        draft.date = std::chrono::year{date.year()} / date.month() / date.day();
        draft.notes =
            sportForm::text(m_checkpoint.editorText, "notes", QString::fromStdString(draft.notes)).toStdString();
        const auto manual = sportForm::text(
            m_checkpoint.editorText, "duration",
            draft.timer.manualDuration
                ? QString::number(std::chrono::duration_cast<std::chrono::seconds>(*draft.timer.manualDuration).count())
                : QString{});
        draft.timer.manualDuration =
            manual.isEmpty() ? std::nullopt
                             : std::optional{std::chrono::duration_cast<core::SportDuration>(std::chrono::seconds{
                                   sportForm::integer(manual, 1, 604800, "Итоговая длительность")})};
        draft.before = readWellbeing(draft.before, m_checkpoint.editorText, "before");
        draft.after = readWellbeing(draft.after, m_checkpoint.editorText, "after");
        for (auto& item : draft.items)
        {
            item.plan.conditions = sportForm::readConditions(item.plan, m_checkpoint.editorText);
            for (auto& actual : item.actual)
            {
                if (actual.state == core::SportSetState::Performed)
                {
                    actual = readActual(actual, m_checkpoint.editorText);
                }
            }
        }
        return draft;
    }
    catch (const std::exception& error)
    {
        return core::SportRejection{"input", error.what()};
    }
}
void SportWorkoutEditor::acknowledge(std::uint64_t sequence)
{
    m_checkpoint.sequence = sequence;
    m_saved = true;
    m_status = "Черновик сохранён на устройстве";
    emit statusChanged();
}
void SportWorkoutEditor::reportError(const QString& message)
{
    m_error = message;
    emit statusChanged();
}
void SportWorkoutEditor::reportStorageError(const QString& message)
{
    m_status = message;
    emit statusChanged();
}
void SportWorkoutEditor::applyTimer(core::SportTimerState timer)
{
    const auto sameAbsences =
        std::ranges::equal(m_checkpoint.draft.timer.absences, timer.absences,
                           [](const auto& previous, const auto& current)
                           {
                               return previous.identifier == current.identifier && previous.begin == current.begin &&
                                      previous.end == current.end && previous.duration == current.duration &&
                                      previous.active == current.active && previous.decision == current.decision;
                           });
    m_liveElapsed = {};
    m_checkpoint.draft.timer = std::move(timer);
    m_saved = false;
    // Тик часов не пересоздаёт поля ввода ручного интервала и не отнимает фокус.
    if (!sameAbsences)
    {
        emit changed();
    }
    emit timeChanged();
}
void SportWorkoutEditor::showElapsed(core::SportDuration elapsed)
{
    m_liveElapsed = elapsed;
    emit timeChanged();
}
void SportWorkoutEditor::applyPrepared(core::SportWorkoutDraft draft)
{
    m_checkpoint.draft = std::move(draft);
    m_saved = false;
}
void SportWorkoutEditor::edited(bool structural)
{
    m_error.clear();
    m_saved = false;
    m_status =
        correcting() ? "Правки не сохранены. Сохраните их перед закрытием приложения." : "Изменения ещё не сохранены";
    if (structural)
    {
        emit changed();
    }
    emit statusChanged();
    if (active())
    {
        emit checkpointRequested();
    }
}
void SportWorkoutEditor::setField(const QString& key, const QString& value)
{
    if (!editing())
    {
        return;
    }
    m_checkpoint.editorText[key.toStdString()] = value.toStdString();
    edited(key.endsWith("/newPain"));
}
bool SportWorkoutEditor::apply(core::SportResult<core::SportWorkoutDraft> result)
{
    if (const auto* error = std::get_if<core::SportRejection>(&result))
    {
        reportError(QString::fromStdString(error->reason));
        return false;
    }
    m_checkpoint.draft = std::get<core::SportWorkoutDraft>(std::move(result));
    edited(true);
    return true;
}
void SportWorkoutEditor::recordSet(const QString& identifier, const QString& setIdentifier)
{
    if (!editing())
    {
        return;
    }
    try
    {
        for (const auto& item : m_checkpoint.draft.items)
        {
            if (item.plan.identifier != identifier.toStdString())
            {
                continue;
            }
            const auto actual =
                std::ranges::find(item.actual, setIdentifier.toStdString(), &core::SportActualSet::identifier);
            if (actual == item.actual.end())
            {
                return;
            }
            apply(core::SportWorkoutService{}.recordSet(m_checkpoint.draft, item.plan.identifier,
                                                        readActual(*actual, m_checkpoint.editorText)));
            return;
        }
    }
    catch (const std::exception& error)
    {
        reportError(QString::fromUtf8(error.what()));
    }
}
void SportWorkoutEditor::skipSet(const QString& identifier, const QString& setIdentifier)
{
    if (editing())
    {
        apply(core::SportWorkoutService{}.skipSet(m_checkpoint.draft, identifier.toStdString(),
                                                  setIdentifier.toStdString()));
    }
}
void SportWorkoutEditor::skipExercise(const QString& identifier)
{
    if (editing())
    {
        apply(core::SportWorkoutService{}.skipExercise(m_checkpoint.draft, identifier.toStdString()));
    }
}
void SportWorkoutEditor::addSet(const QString& identifier, const std::string& setIdentifier)
{
    if (!editing())
    {
        return;
    }
    for (const auto& item : m_checkpoint.draft.items)
    {
        if (item.plan.identifier != identifier.toStdString())
        {
            continue;
        }
        core::SportActualSet actual;
        actual.identifier = setIdentifier;
        actual.load = item.plan.sets.front().load;
        actual.side = item.plan.exercise.sides.front();
        apply(core::SportWorkoutService{}.addSet(m_checkpoint.draft, item.plan.identifier, actual));
        return;
    }
}
void SportWorkoutEditor::replaceExercise(const QString& identifier, const core::SportProgramItem& replacement)
{
    if (editing())
    {
        apply(core::SportWorkoutService{}.replaceExercise(m_checkpoint.draft, identifier.toStdString(), replacement));
    }
}
void SportWorkoutEditor::removeItem(const QString& identifier)
{
    if (!correcting())
    {
        return;
    }
    std::erase_if(m_checkpoint.draft.items,
                  [identifier](const auto& item) { return item.plan.identifier == identifier.toStdString(); });
    edited(true);
}
void SportWorkoutEditor::removeSet(const QString& identifier, const QString& setIdentifier)
{
    if (!correcting())
    {
        return;
    }
    for (auto& item : m_checkpoint.draft.items)
    {
        if (item.plan.identifier == identifier.toStdString())
        {
            std::erase_if(item.actual, [setIdentifier](const auto& actual)
                          { return actual.identifier == setIdentifier.toStdString(); });
        }
    }
    edited(true);
}
void SportWorkoutEditor::toggleArea(const QString& prefix, const QString& area)
{
    if (!editing() || (prefix != "before" && prefix != "after"))
    {
        return;
    }
    const auto& fallback = prefix == "before" ? m_checkpoint.draft.before : m_checkpoint.draft.after;
    auto values = areas(m_checkpoint.editorText, prefix.toStdString(), fallback);
    if (values.contains(area))
    {
        values.removeAll(area);
    }
    else
    {
        values.append(area);
    }
    m_checkpoint.editorText[prefix.toStdString() + "/areas"] = values.join(';').toStdString();
    edited(true);
}
void SportWorkoutEditor::pauseOrResume()
{
    if (active())
    {
        emit pauseRequested();
    }
}
void SportWorkoutEditor::resolveAbsence(const QString& identifier, const QString& decision)
{
    if (active())
    {
        emit absenceRequested(identifier, decision);
    }
}
void SportWorkoutEditor::requestSet(const QString& identifier)
{
    if (editing())
    {
        emit setRequested(identifier);
    }
}
void SportWorkoutEditor::requestReplacement(const QString& identifier, const QString& exercise)
{
    if (editing())
    {
        emit replacementRequested(identifier, exercise);
    }
}
void SportWorkoutEditor::showTechnique(const QString& identifier)
{
    emit techniqueRequested(identifier);
}
void SportWorkoutEditor::retry()
{
    m_error.clear();
    if (active())
    {
        emit checkpointRequested();
    }
}
void SportWorkoutEditor::finish()
{
    if (editing())
    {
        emit finishRequested();
    }
}
void SportWorkoutEditor::resume()
{
    if (active())
    {
        m_visible = true;
        emit changed();
    }
}
void SportWorkoutEditor::close()
{
    if (correcting() && !m_saved)
    {
        m_discardRequested = true;
        emit changed();
        return;
    }
    if (active() && !m_saved)
    {
        reportError("Изменения не сохранены. Повторите локальное сохранение.");
        return;
    }
    m_visible = false;
    emit changed();
}
void SportWorkoutEditor::beginCorrection()
{
    if (!m_original)
    {
        return;
    }
    m_correcting = true;
    m_discardRequested = false;
    m_status = "Исправление результата. Исходные временные отметки сохраняются.";
    emit changed();
    emit statusChanged();
}
void SportWorkoutEditor::keepEditing()
{
    m_discardRequested = false;
    emit changed();
}
void SportWorkoutEditor::discardCorrection()
{
    if (m_original)
    {
        openResult(*m_original);
    }
}
} // namespace msa::ui
