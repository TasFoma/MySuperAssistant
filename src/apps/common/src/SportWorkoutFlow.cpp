#include "msa/client/SportController.h"

#include <QDateTime>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace msa::client
{
namespace
{
constexpr int kTimerTickMilliseconds = 1000;
constexpr int kTimerCheckpointMilliseconds = 5000;
constexpr int kClockToleranceMilliseconds = 5000;
[[nodiscard]] core::SportInstant instant()
{
    return core::SportInstant{core::SportDuration{QDateTime::currentMSecsSinceEpoch()}};
}
template <typename ValueType> [[nodiscard]] ValueType checked(core::SportResult<ValueType> result)
{
    if (const auto* error = std::get_if<core::SportRejection>(&result))
    {
        throw std::runtime_error{error->reason};
    }
    return std::get<ValueType>(std::move(result));
}
} // namespace
void SportController::connectWorkout()
{
    auto* journal = m_model.journal();
    auto* editor = journal->workoutEditor();
    connect(journal, &ui::SportJournalModel::startWorkoutRequested, this, &SportController::onStartWorkout);
    connect(journal, &ui::SportJournalModel::openWorkoutRequested, this, &SportController::onOpenWorkout);
    connect(editor, &ui::SportWorkoutEditor::checkpointRequested, this, &SportController::onWorkoutCheckpoint);
    connect(editor, &ui::SportWorkoutEditor::finishRequested, this, &SportController::onWorkoutFinish);
    connect(editor, &ui::SportWorkoutEditor::pauseRequested, this, &SportController::onWorkoutPause);
    connect(editor, &ui::SportWorkoutEditor::absenceRequested, this, &SportController::onWorkoutAbsence);
    connect(editor, &ui::SportWorkoutEditor::setRequested, this,
            [this](const QString& identifier) { m_model.journal()->workoutEditor()->addSet(identifier, sportId()); });
    connect(editor, &ui::SportWorkoutEditor::replacementRequested, this, &SportController::onWorkoutReplacement);
    connect(editor, &ui::SportWorkoutEditor::techniqueRequested, this,
            [this](const QString& identifier)
            {
                for (const auto& item : m_model.journal()->workoutEditor()->checkpoint().draft.items)
                {
                    if (item.plan.identifier == identifier.toStdString())
                    {
                        m_model.selectSnapshot(item.plan.exercise);
                        return;
                    }
                }
            });
    connect(journal, &ui::SportJournalModel::restrictionsRequested, this,
            [this]
            {
                try
                {
                    if (!m_store)
                    {
                        throw std::runtime_error{"Локальное сохранение недоступно."};
                    }
                    m_store->saveRestrictions(m_model.journal()->restrictions().toStdString());
                    m_model.journal()->reportError("Ограничения сохранены на устройстве.");
                }
                catch (const std::exception& error)
                {
                    m_model.journal()->reportError(
                        QString{"Ограничения не сохранены. %1"}.arg(QString::fromUtf8(error.what())));
                }
            });
    m_workoutTimer.setInterval(kTimerTickMilliseconds);
    connect(&m_workoutTimer, &QTimer::timeout, this,
            [this]
            {
                auto* workout = m_model.journal()->workoutEditor();
                if (!workout->active() || m_absent)
                {
                    return;
                }
                if (m_elapsed.elapsed() >= kTimerCheckpointMilliseconds)
                {
                    onWorkoutCheckpoint();
                }
                else
                {
                    workout->showElapsed(core::SportDuration{m_elapsed.elapsed()});
                }
            });
    auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (application)
    {
        connect(application, &QGuiApplication::applicationStateChanged, this,
                [this](Qt::ApplicationState state)
                {
                    if (state == Qt::ApplicationActive)
                    {
                        resumeWorkout();
                    }
                    else
                    {
                        suspendWorkout();
                    }
                });
        connect(application, &QCoreApplication::aboutToQuit, this, [this] { suspendWorkout(); });
    }
}
void SportController::refreshHistory()
{
    m_model.journal()->applyHistory(m_store->workoutHistory());
}
void SportController::initializeWorkout()
{
    if (m_workoutInitialized)
    {
        return;
    }
    const auto checkpoint = m_store->activeWorkout();
    m_workoutInitialized = true;
    if (!checkpoint)
    {
        return;
    }
    auto* editor = m_model.journal()->workoutEditor();
    editor->openDraft(*checkpoint);
    auto timer = checked(
        core::registerSportAbsence(checkpoint->draft.timer, {sportId(), checkpoint->draft.timer.observed, instant()}));
    editor->applyTimer(std::move(timer));
    m_absent = false;
    m_elapsed.start();
    onWorkoutCheckpoint();
    m_workoutTimer.start();
}
void SportController::onStartWorkout(const QString& identifier, int index)
{
    auto* journal = m_model.journal();
    auto* editor = journal->workoutEditor();
    try
    {
        if (!m_store)
        {
            throw std::runtime_error{"Локальная запись недоступна."};
        }
        if (editor->active())
        {
            throw std::runtime_error{"Уже есть незавершённое занятие. Продолжите его из обзора."};
        }
        if (editor->correcting() && !editor->saved())
        {
            throw std::runtime_error{"Сначала сохраните исправление результата или отмените его."};
        }
        const auto& programs = journal->savedPrograms();
        const auto found =
            std::ranges::find(programs, identifier.toStdString(), &core::SportProgramRevision::identifier);
        if (found == programs.end() || index < 0 || index >= static_cast<int>(found->sessions.size()))
        {
            throw std::runtime_error{"Занятие не найдено."};
        }
        const auto previous = m_store->activeWorkout();
        const auto now = QDateTime::currentDateTime();
        const auto date = now.date();
        auto draft = checked(core::SportWorkoutService{}.start(
            previous ? std::optional{previous->draft} : std::nullopt, found->sessions.at(index), sportId(),
            std::chrono::year{date.year()} / date.month() / date.day(),
            core::SportInstant{core::SportDuration{now.toMSecsSinceEpoch()}}));
        journal->programEditor()->close();
        if (journal->programEditor()->active())
        {
            return;
        }
        draft.restrictionsSnapshot = m_store->restrictions();
        draft.programIdentifier = found->identifier;
        draft.programRevision = found->revision;
        draft.utcOffsetMinutes = now.offsetFromUtc() / 60;
        editor->openDraft({draft, {}, 0});
        m_model.clearSelection();
        m_absent = false;
        m_workoutInitialized = true;
        m_elapsed.start();
        onWorkoutCheckpoint();
        m_workoutTimer.start();
        journal->reportError({});
    }
    catch (const std::exception& error)
    {
        journal->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::observeWorkout()
{
    auto* editor = m_model.journal()->workoutEditor();
    if (!editor->active() || m_absent || !m_elapsed.isValid())
    {
        return;
    }
    const auto elapsed = core::SportDuration{m_elapsed.restart()};
    auto timer = editor->checkpoint().draft.timer;
    const auto current = instant();
    const auto wall = current - timer.observed;
    if (std::abs((wall - elapsed).count()) > kClockToleranceMilliseconds)
    {
        timer.clockWarning = true;
    }
    editor->applyTimer(checked(core::observeSportTimer(timer, {current, elapsed})));
}
void SportController::onWorkoutCheckpoint()
{
    auto* editor = m_model.journal()->workoutEditor();
    if (!editor->active())
    {
        return;
    }
    try
    {
        observeWorkout();
        if (!m_store && !m_databasePath.isEmpty())
        {
            m_store = std::make_unique<infra::SqliteSportStore>(m_databasePath);
        }
        if (!m_store)
        {
            throw std::runtime_error{"Локальное сохранение недоступно."};
        }
        const auto sequence = m_store->checkpoint(editor->checkpoint(), editor->checkpoint().sequence);
        editor->acknowledge(sequence);
    }
    catch (const std::exception& error)
    {
        m_store.reset();
        editor->reportStorageError(QString{"Тренировка не сохранена. %1"}.arg(QString::fromUtf8(error.what())));
    }
}
void SportController::suspendWorkout()
{
    auto* editor = m_model.journal()->workoutEditor();
    if (!editor->active() || m_absent)
    {
        return;
    }
    try
    {
        observeWorkout();
        m_absent = true;
        m_workoutTimer.stop();
        onWorkoutCheckpoint();
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::resumeWorkout()
{
    auto* editor = m_model.journal()->workoutEditor();
    if (!editor->active() || !m_absent)
    {
        return;
    }
    try
    {
        const auto& previous = editor->checkpoint().draft.timer;
        auto timer = checked(core::registerSportAbsence(previous, {sportId(), previous.observed, instant()}));
        editor->applyTimer(std::move(timer));
        m_absent = false;
        m_elapsed.start();
        onWorkoutCheckpoint();
        m_workoutTimer.start();
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::onWorkoutPause()
{
    auto* editor = m_model.journal()->workoutEditor();
    try
    {
        observeWorkout();
        const auto& timer = editor->checkpoint().draft.timer;
        editor->applyTimer(timer.paused ? core::resumeSportTimer(timer) : core::pauseSportTimer(timer));
        onWorkoutCheckpoint();
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::onWorkoutAbsence(const QString& identifier, const QString& decision)
{
    auto* editor = m_model.journal()->workoutEditor();
    try
    {
        std::optional<core::SportDuration> manual;
        core::SportAbsenceDecision choice{};
        if (decision == "activity")
        {
            choice = core::SportAbsenceDecision::Activity;
        }
        else if (decision == "pause")
        {
            choice = core::SportAbsenceDecision::Pause;
        }
        else if (decision == "manual")
        {
            choice = core::SportAbsenceDecision::Manual;
            const auto seconds = ui::sportForm::integer(
                ui::sportForm::text(editor->checkpoint().editorText, identifier.toStdString() + "/activeSeconds"), 0,
                604800, "Активная часть интервала");
            manual = std::chrono::duration_cast<core::SportDuration>(std::chrono::seconds{seconds});
        }
        else
        {
            throw std::runtime_error{"Выберите способ учёта интервала."};
        }
        editor->applyTimer(checked(
            core::resolveSportAbsence(editor->checkpoint().draft.timer, identifier.toStdString(), choice, manual)));
        onWorkoutCheckpoint();
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::onWorkoutReplacement(const QString& identifier, const QString& exerciseIdentifier)
{
    auto* editor = m_model.journal()->workoutEditor();
    try
    {
        const auto exercise =
            std::ranges::find(m_catalog, exerciseIdentifier.toStdString(), &core::SportExerciseRevision::identifier);
        if (exercise == m_catalog.end() || !exercise->published)
        {
            return;
        }
        const auto& items = editor->checkpoint().draft.items;
        const auto found =
            std::ranges::find(items, identifier.toStdString(), [](const auto& item) { return item.plan.identifier; });
        if (found == items.end())
        {
            return;
        }
        core::SportProgramItem replacement;
        replacement.identifier = sportId();
        replacement.exercise = *exercise;
        const auto& text = editor->checkpoint().editorText;
        const auto prefix = identifier.toStdString();
        const auto sets =
            ui::sportForm::integer(ui::sportForm::text(text, prefix + "/replacementSets"), 1, 100, "Подходы замены");
        const auto repetitions = ui::sportForm::integer(ui::sportForm::text(text, prefix + "/replacementReps"), 1, 1000,
                                                        "Повторения замены");
        // План замены задаёт пользователь; вес другого упражнения не переносится.
        for (int index = 0; index < sets; ++index)
        {
            core::SportPlannedSet planned;
            planned.identifier = sportId();
            planned.repetitions = repetitions;
            planned.side = exercise->sides.front();
            if (exercise->loadKind == 1)
            {
                planned.load = core::DumbbellLoad{std::nullopt, exercise->requirements.dumbbellQuantity.value_or(2)};
            }
            if (exercise->loadKind == 2)
            {
                planned.load = core::BarbellLoad{};
            }
            replacement.sets.push_back(planned);
        }
        editor->replaceExercise(identifier, replacement);
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString::fromUtf8(error.what()));
    }
}
void SportController::onWorkoutFinish()
{
    auto* editor = m_model.journal()->workoutEditor();
    try
    {
        if (!m_store)
        {
            throw std::runtime_error{"Локальное сохранение недоступно."};
        }
        if (editor->active())
        {
            observeWorkout();
        }
        const auto candidate = checked(editor->candidate());
        core::SportWorkoutRevision revision;
        if (editor->correcting())
        {
            const auto& original = *editor->original();
            revision =
                checked(core::SportWorkoutService{}.correct(original, original.identifier, candidate, sportId()));
            m_store->correctWorkout(revision, original.identifier);
        }
        else if (editor->active())
        {
            // Проверяем готовность до checkpoint, затем сохраняем точно его последнюю версию.
            static_cast<void>(checked(core::SportWorkoutService{}.finish(candidate, "validation")));
            editor->applyPrepared(candidate);
            onWorkoutCheckpoint();
            if (!editor->saved())
            {
                return;
            }
            revision = checked(core::SportWorkoutService{}.finish(editor->checkpoint().draft, sportId()));
            m_store->finishWorkout(revision, editor->checkpoint().sequence);
        }
        else
        {
            return;
        }
        m_workoutTimer.stop();
        editor->openResult(revision);
        m_model.clearSelection();
        m_model.navigate("history");
        try
        {
            refreshHistory();
        }
        catch (const std::exception& error)
        {
            m_model.journal()->reportError(
                QString{"Результат сохранён, но история не прочитана. %1"}.arg(QString::fromUtf8(error.what())));
        }
    }
    catch (const std::exception& error)
    {
        editor->reportError(QString{"Результат не сохранён. %1"}.arg(QString::fromUtf8(error.what())));
    }
}
void SportController::onOpenWorkout(const QString& identifier)
{
    auto* journal = m_model.journal();
    auto* editor = journal->workoutEditor();
    if (editor->active())
    {
        journal->reportError("Сначала завершите текущее занятие. Оно доступно из обзора.");
        return;
    }
    if (editor->correcting() && !editor->saved())
    {
        journal->reportError("Сначала сохраните или отмените открытое исправление.");
        return;
    }
    try
    {
        refreshHistory();
    }
    catch (const std::exception& error)
    {
        journal->reportError(
            QString{"Не удалось прочитать актуальные результаты. %1"}.arg(QString::fromUtf8(error.what())));
        return;
    }
    for (const auto& record : journal->savedHistory())
    {
        if (record.current.workout.identifier != identifier.toStdString())
        {
            continue;
        }
        journal->programEditor()->close();
        if (journal->programEditor()->active())
        {
            return;
        }
        editor->openResult(record.current);
        m_model.clearSelection();
        m_model.navigate("history");
        return;
    }
}
} // namespace msa::client
