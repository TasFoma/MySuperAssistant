#include "msa/client/SportController.h"

#include "msa/infra/SportCatalog.h"

#include <QUuid>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace msa::client
{
std::string SportController::sportId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
SportController::SportController(ui::SportLibraryModel& model, QString databasePath, QObject* parent) :
    QObject{parent}, m_model{model}, m_databasePath{std::move(databasePath)}
{
    connect(&m_model, &ui::SportLibraryModel::preferenceRequested, this, &SportController::onPreferenceRequested);
    connect(&m_model, &ui::SportLibraryModel::reloadRequested, this, &SportController::start);
    connectWorkout();
    auto* journal = m_model.journal();
    auto* editor = journal->programEditor();
    connect(journal, &ui::SportJournalModel::createProgramRequested, this, &SportController::onCreateProgram);
    connect(journal, &ui::SportJournalModel::editProgramRequested, this,
            [this](const QString& identifier) { onEditProgram(identifier, false); });
    connect(journal, &ui::SportJournalModel::copyProgramRequested, this,
            [this](const QString& identifier) { onEditProgram(identifier, true); });
    connect(journal, &ui::SportJournalModel::resumeProgramRequested, this, &SportController::onResumeProgram);
    connect(editor, &ui::SportProgramEditor::checkpointRequested, this, &SportController::onProgramCheckpoint);
    connect(editor, &ui::SportProgramEditor::saveRequested, this, &SportController::onProgramSave);
    connect(editor, &ui::SportProgramEditor::sessionRequested, this,
            [this] { m_model.journal()->programEditor()->addSession(sportId()); });
    connect(editor, &ui::SportProgramEditor::setRequested, this,
            [this](const QString& identifier) { m_model.journal()->programEditor()->addSet(identifier, sportId()); });
    connect(editor, &ui::SportProgramEditor::exerciseRequested, this, &SportController::onAddExercise);
    connect(editor, &ui::SportProgramEditor::techniqueRequested, this,
            [this](const QString& identifier)
            {
                for (const auto& session : m_model.journal()->programEditor()->checkpoint().program.sessions)
                {
                    for (const auto& item : session.items)
                    {
                        if (item.identifier == identifier.toStdString())
                        {
                            m_model.selectSnapshot(item.exercise);
                            return;
                        }
                    }
                }
            });
}
void SportController::start()
{
    std::vector<core::SportExerciseRevision> catalog;
    try
    {
        if (m_model.journal()->programEditor()->active() && !m_model.journal()->programEditor()->saved())
        {
            onProgramCheckpoint();
            return;
        }
        catalog = infra::loadBundledSportCatalog();
        m_catalog = catalog;
        m_model.applyCatalog(catalog);
        if (m_databasePath.isEmpty())
        {
            m_model.applyData(catalog, {}, false);
            return;
        }
        if (m_model.journal()->workoutEditor()->active() && !m_model.journal()->workoutEditor()->saved())
        {
            onWorkoutCheckpoint();
            return;
        }
        m_store.reset();
        m_store = std::make_unique<infra::SqliteSportStore>(m_databasePath);
        m_store->importExercises(catalog);
        m_catalog = m_store->exercises();
        m_model.applyData(m_catalog, m_store->preferences(), true);
        refreshPrograms();
        refreshHistory();
        m_model.journal()->applyRestrictions(QString::fromStdString(m_store->restrictions()));
        initializeWorkout();
    }
    catch (const std::exception& error)
    {
        m_store.reset();
        m_model.reportStorageError(
            QString{"Не удалось прочитать локальные данные спорта. %1"}.arg(QString::fromUtf8(error.what())));
    }
}
void SportController::onPreferenceRequested(const QString& identifier, bool favorite, bool progression)
{
    try
    {
        if (!m_store)
        {
            m_model.reportError("Локальное сохранение недоступно. Повторите загрузку.");
            return;
        }
        const core::SportExercisePreference value{favorite, progression};
        m_store->savePreference(identifier.toStdString(), value);
        m_model.applyPreference(identifier.toStdString(), value);
    }
    catch (const std::exception& error)
    {
        m_model.reportError(QString{"Отметка не сохранена. %1"}.arg(QString::fromUtf8(error.what())));
    }
}

void SportController::refreshPrograms()
{
    m_model.journal()->applyPrograms(m_store->programs(), m_store->programDrafts());
}
void SportController::onCreateProgram()
{
    auto* editor = m_model.journal()->programEditor();
    const auto* workout = m_model.journal()->workoutEditor();
    if (workout->visible() && !workout->saved())
    {
        m_model.journal()->reportError("Сначала сохраните открытое занятие или исправление.");
        return;
    }
    if (!m_store || (editor->active() && !editor->saved()))
    {
        m_model.journal()->reportError("Сначала сохраните открытый черновик.");
        return;
    }
    core::SportProgramCheckpoint checkpoint;
    checkpoint.editorText["_baseRevision"] = "";
    checkpoint.program.identifier = sportId();
    checkpoint.program.revision = sportId();
    checkpoint.program.sessions.push_back({sportId(), {}, {}});
    editor->open(checkpoint, std::nullopt);
    m_model.clearSelection();
    m_model.journal()->workoutEditor()->close();
    m_model.openPrograms();
    onProgramCheckpoint();
}
void SportController::onResumeProgram(const QString& identifier)
{
    auto* editor = m_model.journal()->programEditor();
    const auto* workout = m_model.journal()->workoutEditor();
    if (workout->visible() && !workout->saved())
    {
        m_model.journal()->reportError("Сначала сохраните открытое занятие или исправление.");
        return;
    }
    if (!m_store || (editor->active() && !editor->saved()))
    {
        return;
    }
    for (const auto& draft : m_model.journal()->savedDrafts())
    {
        if (draft.program.identifier != identifier.toStdString())
        {
            continue;
        }
        std::optional<std::string> expected;
        const auto baseline = draft.editorText.find("_baseRevision");
        if (baseline != draft.editorText.end())
        {
            if (!baseline->second.empty())
            {
                expected = baseline->second;
            }
        }
        else
        {
            for (const auto& program : m_model.journal()->savedPrograms())
            {
                if (program.identifier == draft.program.identifier)
                {
                    expected = "unknown-base-revision";
                }
            }
        }

        editor->open(draft, expected);
        m_model.journal()->workoutEditor()->close();
        m_model.clearSelection();
        m_model.openPrograms();
        return;
    }
}
void SportController::onEditProgram(const QString& identifier, bool copy)
{
    auto* editor = m_model.journal()->programEditor();
    const auto* workout = m_model.journal()->workoutEditor();
    if (workout->visible() && !workout->saved())
    {
        m_model.journal()->reportError("Сначала сохраните открытое занятие или исправление.");
        return;
    }
    if (!m_store || (editor->active() && !editor->saved()))
    {
        return;
    }
    const auto& programs = m_model.journal()->savedPrograms();
    const auto found = std::ranges::find(programs, identifier.toStdString(), &core::SportProgramRevision::identifier);
    if (found == programs.end())
    {
        return;
    }
    if (!copy)
    {
        for (const auto& draft : m_model.journal()->savedDrafts())
        {
            if (draft.program.identifier == found->identifier && draft.program.revision != found->revision)
            {
                onResumeProgram(identifier);
                return;
            }
        }
    }
    core::SportProgramCheckpoint checkpoint;
    checkpoint.editorText["_baseRevision"] = copy ? "" : found->revision;
    checkpoint.program = *found;
    checkpoint.program.revision = sportId();
    if (copy)
    {
        checkpoint.program.identifier = sportId();
        checkpoint.program.sourceRevision = found->revision;
        checkpoint.program.title += " — копия";
        for (auto& session : checkpoint.program.sessions)
        {
            session.identifier = sportId();
            for (auto& item : session.items)
            {
                item.identifier = sportId();
                for (auto& planned : item.sets)
                {
                    planned.identifier = sportId();
                }
            }
        }
    }
    else
    {
        for (const auto& draft : m_model.journal()->savedDrafts())
        {
            if (draft.program.identifier == found->identifier)
            {
                checkpoint.sequence = draft.sequence;
            }
        }
    }
    editor->open(checkpoint, copy ? std::nullopt : std::optional{found->revision});
    m_model.clearSelection();
    m_model.journal()->workoutEditor()->close();
    m_model.openPrograms();
    onProgramCheckpoint();
}
void SportController::onProgramCheckpoint()
{
    auto* editor = m_model.journal()->programEditor();
    try
    {
        if (!m_store && !m_databasePath.isEmpty())
        {
            m_store = std::make_unique<infra::SqliteSportStore>(m_databasePath);
        }
        if (!m_store)
        {
            throw std::runtime_error{"Локальная запись недоступна."};
        }
        const auto sequence = m_store->checkpointProgram(editor->checkpoint(), editor->checkpoint().sequence);
        editor->acknowledge(sequence);
        try
        {
            refreshPrograms();
        }
        catch (const std::exception& error)
        {
            m_model.journal()->reportError(
                QString{"Черновик сохранён, но список не прочитан. %1"}.arg(QString::fromUtf8(error.what())));
        }
    }
    catch (const std::exception& error)
    {
        m_store.reset();
        editor->reportError(QString{"Черновик не сохранён. %1"}.arg(QString::fromUtf8(error.what())));
    }
}
void SportController::onProgramSave()
{
    auto* editor = m_model.journal()->programEditor();
    try
    {
        const auto candidate = editor->candidate();
        if (const auto* error = std::get_if<core::SportRejection>(&candidate))
        {
            editor->reportError(QString::fromStdString(error->reason));
            return;
        }
        onProgramCheckpoint();
        if (!editor->saved())
        {
            return;
        }
        m_store->saveProgram(std::get<core::SportProgramRevision>(candidate), editor->expectedRevision());
        editor->close();
        try
        {
            refreshPrograms();
        }
        catch (const std::exception& error)
        {
            m_model.journal()->reportError(
                QString{"Программа сохранена, но список не прочитан. %1"}.arg(QString::fromUtf8(error.what())));
        }
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString{"Программа не сохранена. %1"}.arg(QString::fromUtf8(error.what())));
    }
}
void SportController::onAddExercise(const QString& identifier)
{
    const auto found = std::ranges::find(m_catalog, identifier.toStdString(), &core::SportExerciseRevision::identifier);
    if (found == m_catalog.end() || !found->published)
    {
        return;
    }
    m_model.journal()->programEditor()->addExercise(*found, sportId(), sportId());
}
} // namespace msa::client
