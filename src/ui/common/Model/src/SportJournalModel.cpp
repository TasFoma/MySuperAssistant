#include "msa/ui/SportJournalModel.h"

#include "msa/core/SportProgress.h"

#include <QDate>
#include <algorithm>

namespace msa::ui
{
SportJournalModel::SportJournalModel(QObject* parent) : QObject{parent} {}
SportProgramEditor* SportJournalModel::programEditor()
{
    return &m_programEditor;
}
const std::vector<core::SportProgramRevision>& SportJournalModel::savedPrograms() const
{
    return m_programs;
}
const std::vector<core::SportProgramCheckpoint>& SportJournalModel::savedDrafts() const
{
    return m_drafts;
}
QString SportJournalModel::errorText() const
{
    return m_error;
}
void SportJournalModel::reportError(const QString& text)
{
    m_error = text;
    emit changed();
}
void SportJournalModel::applyPrograms(std::vector<core::SportProgramRevision> programs,
                                      std::vector<core::SportProgramCheckpoint> drafts)
{
    m_programs = std::move(programs);
    m_drafts = std::move(drafts);
    m_error.clear();
    emit changed();
}
QVariantList SportJournalModel::programs() const
{
    QVariantList result;
    for (const auto& program : m_programs)
    {
        QVariantList sessions;
        int index{};
        for (const auto& session : program.sessions)
        {
            sessions.append(QVariantMap{{"index", index++},
                                        {"title", QString::fromStdString(session.title)},
                                        {"count", static_cast<int>(session.items.size())}});
        }
        result.append(QVariantMap{{"identifier", QString::fromStdString(program.identifier)},
                                  {"title", QString::fromStdString(program.title)},
                                  {"sessions", sessions}});
    }
    return result;
}
QVariantList SportJournalModel::drafts() const
{
    QVariantList result;
    for (const auto& draft : m_drafts)
    {
        const auto published = std::ranges::any_of(
            m_programs, [draft](const auto& program)
            { return program.identifier == draft.program.identifier && program.revision == draft.program.revision; });
        if (published)
        {
            continue;
        }
        const auto title = sportForm::text(draft.editorText, "title", QString::fromStdString(draft.program.title));
        result.append(QVariantMap{{"identifier", QString::fromStdString(draft.program.identifier)},
                                  {"title", title.isEmpty() ? "Без названия" : title}});
    }
    return result;
}
void SportJournalModel::createProgram()
{
    emit createProgramRequested();
}
void SportJournalModel::editProgram(const QString& identifier)
{
    emit editProgramRequested(identifier);
}
void SportJournalModel::copyProgram(const QString& identifier)
{
    emit copyProgramRequested(identifier);
}
void SportJournalModel::resumeProgram(const QString& identifier)
{
    emit resumeProgramRequested(identifier);
}
void SportJournalModel::startWorkout(const QString& identifier, int index)
{
    emit startWorkoutRequested(identifier, index);
}

SportWorkoutEditor* SportJournalModel::workoutEditor()
{
    return &m_workoutEditor;
}
const std::vector<core::SportWorkoutRecord>& SportJournalModel::savedHistory() const
{
    return m_history;
}
void SportJournalModel::applyHistory(std::vector<core::SportWorkoutRecord> values)
{
    m_error.clear();
    m_history = std::move(values);
    emit changed();
}
QString SportJournalModel::restrictions() const
{
    return m_restrictions;
}
void SportJournalModel::setRestrictions(const QString& text)
{
    m_restrictions = text;
}
void SportJournalModel::applyRestrictions(const QString& text)
{
    m_restrictions = text;
    emit restrictionsChanged();
}
void SportJournalModel::saveRestrictions()
{
    emit restrictionsRequested();
}
void SportJournalModel::openWorkout(const QString& identifier)
{
    emit openWorkoutRequested(identifier);
}
QVariantList SportJournalModel::history() const
{
    QVariantList result;
    for (const auto& record : m_history)
    {
        const auto& workout = record.current.workout;
        const auto date =
            QDate{static_cast<int>(workout.date.year()), static_cast<int>(static_cast<unsigned>(workout.date.month())),
                  static_cast<int>(static_cast<unsigned>(workout.date.day()))}
                .toString(Qt::ISODate);
        result.append(
            QVariantMap{{"identifier", QString::fromStdString(workout.identifier)},
                        {"title", date + " · " + QString::fromStdString(workout.originalPlan.title)},
                        {"status", record.current.entirelySkipped ? "Полностью пропущено · сохранено на устройстве"
                                                                  : "Выполнено · сохранено на устройстве"}});
    }
    return result;
}
QVariantList SportJournalModel::progress() const
{
    QVariantList result;
    const auto projected = core::buildSportProgress(m_history);
    if (!std::holds_alternative<std::vector<core::SportProgressSeries>>(projected))
    {
        return result;
    }
    for (const auto& series : std::get<std::vector<core::SportProgressSeries>>(projected))
    {
        if (!m_progressExercise.isEmpty() && series.key.exerciseIdentifier != m_progressExercise.toStdString())
        {
            continue;
        }
        QString title = QString::fromStdString(series.key.exerciseIdentifier);
        for (const auto& record : m_history)
        {
            for (const auto& item : record.current.workout.items)
            {
                if (item.plan.exercise.identifier == series.key.exerciseIdentifier)
                {
                    title = QString::fromStdString(item.plan.exercise.title);
                }
            }
        }
        QString conditions =
            sportForm::side(series.key.side) + " · " + QString::fromStdString(series.key.conditions.amplitude);
        if (series.key.conditions.supportHeightMillimeters)
        {
            conditions += QString{" · опора %1 мм"}.arg(*series.key.conditions.supportHeightMillimeters);
        }
        if (series.key.conditions.wallDistanceMillimeters)
        {
            conditions += QString{" · расстояние %1 мм"}.arg(*series.key.conditions.wallDistanceMillimeters);
        }
        if (!series.key.conditions.note.empty())
        {
            conditions += " · " + QString::fromStdString(series.key.conditions.note);
        }
        if (!series.key.unknownContext.empty())
        {
            conditions += " · условия не подтверждены, отдельная серия";
        }
        QVariantList points;
        for (const auto& point : series.results)
        {
            const auto date =
                QDate{static_cast<int>(point.date.year()), static_cast<int>(static_cast<unsigned>(point.date.month())),
                      static_cast<int>(static_cast<unsigned>(point.date.day()))}
                    .toString(Qt::ISODate);
            points.append(QVariantMap{{"workoutId", QString::fromStdString(point.workoutIdentifier)},
                                      {"text", QString{"%1 · %2 повт. · %3"}
                                                   .arg(date)
                                                   .arg(*point.actual.repetitions)
                                                   .arg(sportForm::load(point.actual.load))}});
        }
        result.append(QVariantMap{{"title", title}, {"conditions", conditions}, {"points", points}});
    }
    return result;
}

QString SportJournalModel::progressExercise() const
{
    return m_progressExercise;
}
void SportJournalModel::setProgressExercise(const QString& identifier)
{
    m_progressExercise = identifier;
    emit changed();
}
QVariantList SportJournalModel::progressExercises() const
{
    QVariantList result{QVariantMap{{"value", ""}, {"label", "Все упражнения"}}};
    std::map<std::string, std::string> exercises;
    for (const auto& record : m_history)
    {
        for (const auto& item : record.current.workout.items)
        {
            exercises[item.plan.exercise.identifier] = item.plan.exercise.title;
        }
    }
    for (const auto& [identifier, title] : exercises)
    {
        result.append(
            QVariantMap{{"value", QString::fromStdString(identifier)}, {"label", QString::fromStdString(title)}});
    }
    return result;
}
} // namespace msa::ui
