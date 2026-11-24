#include "msa/ui/SportProgramEditor.h"

#include <algorithm>
#include <stdexcept>

namespace msa::ui
{
SportProgramEditor::SportProgramEditor(QObject* parent) : QObject{parent}
{
    connect(this, &SportProgramEditor::changed, this, &SportProgramEditor::sessionsChanged);
}
void SportProgramEditor::open(core::SportProgramCheckpoint checkpoint, std::optional<std::string> expected)
{
    m_checkpoint = std::move(checkpoint);
    m_expected = std::move(expected);
    m_sessionIndex = 0;
    m_active = true;
    m_saved = m_checkpoint.sequence > 0;
    m_status = m_saved ? "Черновик сохранён на устройстве" : "Черновик ещё не сохранён";
    emit changed();
    emit statusChanged();
}
bool SportProgramEditor::active() const
{
    return m_active;
}
bool SportProgramEditor::saved() const
{
    return m_saved;
}
int SportProgramEditor::sessionIndex() const
{
    return m_sessionIndex;
}
QString SportProgramEditor::status() const
{
    return m_status;
}
const core::SportProgramCheckpoint& SportProgramEditor::checkpoint() const
{
    return m_checkpoint;
}
const std::optional<std::string>& SportProgramEditor::expectedRevision() const
{
    return m_expected;
}
QVariantList SportProgramEditor::fields() const
{
    const auto& program = m_checkpoint.program;
    QVariantList result{
        sportForm::field("title", "Название программы",
                         sportForm::text(m_checkpoint.editorText, "title", QString::fromStdString(program.title)))};
    if (m_sessionIndex < static_cast<int>(program.sessions.size()))
    {
        const auto& session = program.sessions.at(m_sessionIndex);
        result.append(sportForm::field(session.identifier + "/title", "Название занятия",
                                       sportForm::text(m_checkpoint.editorText, session.identifier + "/title",
                                                       QString::fromStdString(session.title))));
    }
    return result;
}
QVariantList SportProgramEditor::sessions() const
{
    QVariantList result;
    int index{};
    for (const auto& session : m_checkpoint.program.sessions)
    {
        result.append(QVariantMap{{"index", index++},
                                  {"title", sportForm::text(m_checkpoint.editorText, session.identifier + "/title",
                                                            QString::fromStdString(session.title))}});
    }
    return result;
}
QVariantList SportProgramEditor::items() const
{
    QVariantList result;
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return result;
    }
    for (const auto& item : m_checkpoint.program.sessions.at(m_sessionIndex).items)
    {
        QVariantList sets;
        for (const auto& planned : item.sets)
        {
            auto fields = sportForm::setFields(planned.identifier, planned.load, m_checkpoint.editorText,
                                               QString::number(planned.repetitions));
            if (item.exercise.requirements.dumbbellQuantity)
            {
                const auto required = *item.exercise.requirements.dumbbellQuantity;
                for (auto& entry : fields)
                {
                    auto descriptor = entry.toMap();
                    if (!descriptor.value("key").toString().endsWith("/quantity"))
                    {
                        continue;
                    }
                    descriptor["label"] = QString{"Количество гантелей по карточке: %1"}.arg(required);
                    descriptor["readOnly"] = descriptor.value("value").toString() == QString::number(required);
                    entry = descriptor;
                }
            }
            if (item.exercise.sides.size() > 1)
            {
                fields.append(sportForm::sideField(planned.identifier, planned.side, item.exercise.sides,
                                                   m_checkpoint.editorText));
            }
            sets.append(QVariantMap{{"identifier", QString::fromStdString(planned.identifier)},
                                    {"side", sportForm::side(planned.side)},
                                    {"fields", fields}});
        }
        result.append(QVariantMap{{"identifier", QString::fromStdString(item.identifier)},
                                  {"exerciseId", QString::fromStdString(item.exercise.identifier)},
                                  {"title", QString::fromStdString(item.exercise.title)},
                                  {"conditions", sportForm::conditionFields(item, m_checkpoint.editorText)},
                                  {"sets", sets}});
    }
    return result;
}
core::SportResult<core::SportProgramRevision> SportProgramEditor::candidate() const
{
    try
    {
        auto program = m_checkpoint.program;
        program.title = sportForm::text(m_checkpoint.editorText, "title", QString::fromStdString(program.title))
                            .trimmed()
                            .toStdString();
        for (auto& session : program.sessions)
        {
            session.title = sportForm::text(m_checkpoint.editorText, session.identifier + "/title",
                                            QString::fromStdString(session.title))
                                .trimmed()
                                .toStdString();
            for (auto& item : session.items)
            {
                item.conditions = sportForm::readConditions(item, m_checkpoint.editorText);
                for (auto& planned : item.sets)
                {
                    planned = sportForm::readSet(planned, m_checkpoint.editorText);
                }
            }
        }
        if (const auto error = core::validateSportProgram(program))
        {
            return *error;
        }
        return program;
    }
    catch (const std::exception& error)
    {
        return core::SportRejection{"input", error.what()};
    }
}
void SportProgramEditor::acknowledge(std::uint64_t sequence)
{
    m_checkpoint.sequence = sequence;
    m_saved = true;
    m_status = "Черновик сохранён на устройстве";
    emit statusChanged();
}
void SportProgramEditor::reportError(const QString& message)
{
    m_status = message;
    emit statusChanged();
}
void SportProgramEditor::edited(bool structural)
{
    m_saved = false;
    m_status = "Изменения ещё не сохранены";
    if (structural)
    {
        emit changed();
    }
    emit statusChanged();
    emit checkpointRequested();
}
void SportProgramEditor::setField(const QString& key, const QString& value)
{
    m_checkpoint.editorText[key.toStdString()] = value.toStdString();
    if (key.endsWith("/title"))
    {
        emit sessionsChanged();
    }
    edited(false);
}
void SportProgramEditor::addSession(const std::string& identifier)
{
    m_checkpoint.program.sessions.push_back({identifier, {}, {}});
    m_sessionIndex = static_cast<int>(m_checkpoint.program.sessions.size()) - 1;
    edited(true);
}
void SportProgramEditor::addExercise(const core::SportExerciseRevision& exercise, const std::string& itemIdentifier,
                                     const std::string& setIdentifier)
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()) || !exercise.published)
    {
        return;
    }
    core::SportProgramItem item;
    item.identifier = itemIdentifier;
    item.exercise = exercise;
    core::SportPlannedSet planned;
    planned.identifier = setIdentifier;
    planned.side = exercise.sides.front();
    if (exercise.loadKind == 1)
    {
        planned.load = core::DumbbellLoad{std::nullopt, exercise.requirements.dumbbellQuantity.value_or(2)};
    }
    if (exercise.loadKind == 2)
    {
        planned.load = core::BarbellLoad{};
    }
    item.sets.push_back(planned);
    m_checkpoint.editorText[setIdentifier + "/reps"] = "";
    m_checkpoint.program.sessions.at(m_sessionIndex).items.push_back(item);
    edited(true);
}
void SportProgramEditor::addSet(const QString& identifier, const std::string& setIdentifier)
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return;
    }
    for (auto& item : m_checkpoint.program.sessions.at(m_sessionIndex).items)
    {
        if (item.identifier != identifier.toStdString() || item.sets.size() >= 100)
        {
            continue;
        }
        core::SportPlannedSet planned;
        if (!item.sets.empty())
        {
            planned = item.sets.back();
        }
        else
        {
            planned.side = item.exercise.sides.front();
            if (item.exercise.loadKind == 1)
            {
                planned.load =
                    core::DumbbellLoad{std::nullopt, item.exercise.requirements.dumbbellQuantity.value_or(2)};
            }
            if (item.exercise.loadKind == 2)
            {
                planned.load = core::BarbellLoad{};
            }
        }
        planned.identifier = setIdentifier;
        item.sets.push_back(planned);
        m_checkpoint.editorText[setIdentifier + "/reps"] = "";
        edited(true);
        return;
    }
}
void SportProgramEditor::selectSession(int index)
{
    if (index < 0 || index >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return;
    }
    m_sessionIndex = index;
    emit changed();
}
void SportProgramEditor::removeItem(const QString& identifier)
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return;
    }
    auto& items = m_checkpoint.program.sessions.at(m_sessionIndex).items;
    std::erase_if(items, [identifier](const auto& item) { return item.identifier == identifier.toStdString(); });
    edited(true);
}
void SportProgramEditor::moveItem(const QString& identifier, int direction)
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()) || (direction != -1 && direction != 1))
    {
        return;
    }
    auto& items = m_checkpoint.program.sessions.at(m_sessionIndex).items;
    const auto item = std::ranges::find(items, identifier.toStdString(), &core::SportProgramItem::identifier);
    if (item == items.end())
    {
        return;
    }
    const auto index = std::distance(items.begin(), item);
    if (index + direction < 0 || index + direction >= static_cast<int>(items.size()))
    {
        return;
    }
    std::iter_swap(item, item + direction);
    edited(true);
}
void SportProgramEditor::removeSet(const QString& identifier, const QString& setIdentifier)
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return;
    }
    for (auto& item : m_checkpoint.program.sessions.at(m_sessionIndex).items)
    {
        if (item.identifier == identifier.toStdString())
        {
            std::erase_if(item.sets, [setIdentifier](const auto& planned)
                          { return planned.identifier == setIdentifier.toStdString(); });
        }
    }
    edited(true);
}
void SportProgramEditor::removeSession()
{
    if (m_sessionIndex >= static_cast<int>(m_checkpoint.program.sessions.size()))
    {
        return;
    }
    m_checkpoint.program.sessions.erase(m_checkpoint.program.sessions.begin() + m_sessionIndex);
    m_sessionIndex = 0;
    edited(true);
}
void SportProgramEditor::moveSession(int direction)
{
    auto& sessions = m_checkpoint.program.sessions;
    if ((direction != -1 && direction != 1) || m_sessionIndex + direction < 0 ||
        m_sessionIndex + direction >= static_cast<int>(sessions.size()))
    {
        return;
    }
    std::swap(sessions.at(m_sessionIndex), sessions.at(m_sessionIndex + direction));
    m_sessionIndex += direction;
    edited(true);
}
void SportProgramEditor::close()
{
    if (!m_saved)
    {
        reportError("Изменения не сохранены. Повторите сохранение черновика.");
        return;
    }
    m_active = false;
    emit changed();
}
void SportProgramEditor::save()
{
    emit saveRequested();
}
void SportProgramEditor::retry()
{
    emit checkpointRequested();
}
void SportProgramEditor::requestSession()
{
    emit sessionRequested();
}
void SportProgramEditor::requestExercise(const QString& identifier)
{
    emit exerciseRequested(identifier);
}
void SportProgramEditor::requestSet(const QString& identifier)
{
    emit setRequested(identifier);
}
void SportProgramEditor::showTechnique(const QString& identifier)
{
    emit techniqueRequested(identifier);
}
} // namespace msa::ui
