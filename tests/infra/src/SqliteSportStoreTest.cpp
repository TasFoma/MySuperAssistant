#include "SportTestData.h"
#include "msa/core/SportProgress.h"
#include "msa/infra/SportDocumentCodec.h"
#include "msa/infra/SqliteSportStore.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <stdexcept>

namespace
{
class InMemorySportStore final : public msa::core::SportWorkoutRepository,
                                 public msa::core::SportProgramRepository,
                                 public msa::core::SportPreferencesRepository
{
public:
    std::optional<msa::core::SportCheckpoint> activeWorkout() const override
    {
        return m_active;
    }
    std::uint64_t checkpoint(const msa::core::SportCheckpoint& value, std::uint64_t expected) override
    {
        if ((m_active && (m_active->sequence != expected || m_active->draft.identifier != value.draft.identifier)) ||
            (!m_active && expected != 0))
        {
            throw std::runtime_error{"Конфликт черновика"};
        }
        m_active = value;
        m_active->sequence = expected + 1;
        return m_active->sequence;
    }
    void finishWorkout(const msa::core::SportWorkoutRevision& value, std::uint64_t expected) override
    {
        if (m_records.contains(value.workout.identifier))
        {
            if (m_records.at(value.workout.identifier).current.identifier != value.identifier)
            {
                throw std::runtime_error{"Конфликт"};
            }
            return;
        }
        if (!m_active || m_active->sequence != expected || m_active->draft.identifier != value.workout.identifier)
        {
            throw std::runtime_error{"Конфликт"};
        }
        m_records[value.workout.identifier] = {value};
        m_active.reset();
    }
    void correctWorkout(const msa::core::SportWorkoutRevision& value, const std::string& expected,
                        std::optional<std::uint64_t> expectedDraftSequence = std::nullopt,
                        const std::optional<std::string>& expectedEditing = std::nullopt) override
    {
        if (!m_records.contains(value.workout.identifier) ||
            m_records.at(value.workout.identifier).current.identifier != expected)
        {
            throw std::runtime_error{"Конфликт"};
        }
        if (expectedDraftSequence)
        {
            const auto found = m_corrections.find(value.workout.identifier);
            if (found == m_corrections.end() || !expectedEditing || found->second.identifier != *expectedEditing ||
                found->second.edited.sequence != *expectedDraftSequence)
            {
                throw std::runtime_error{"Конфликт исправления"};
            }
            m_corrections.erase(found);
        }
        m_records[value.workout.identifier] = {value};
    }
    std::uint64_t checkpointCorrection(const msa::core::SportCorrectionCheckpoint& value,
                                       std::uint64_t expected) override
    {
        const auto found = m_corrections.find(value.original.workout.identifier);
        if ((found != m_corrections.end() &&
             (found->second.identifier != value.identifier || found->second.edited.sequence != expected ||
              found->second.original.identifier != value.original.identifier)) ||
            (found == m_corrections.end() && expected != 0))
        {
            throw std::runtime_error{"Конфликт"};
        }
        auto saved = value;
        saved.edited.sequence = expected + 1;
        m_corrections[value.original.workout.identifier] = saved;
        return saved.edited.sequence;
    }
    std::vector<msa::core::SportCorrectionCheckpoint> correctionDrafts() const override
    {
        std::vector<msa::core::SportCorrectionCheckpoint> result;
        for (const auto& [key, value] : m_corrections)
        {
            static_cast<void>(key);
            result.push_back(value);
        }
        return result;
    }
    void discardCorrection(const std::string& workout, const std::string& editing, std::uint64_t expected) override
    {
        const auto found = m_corrections.find(workout);
        if (found == m_corrections.end())
        {
            return;
        }
        if (found->second.identifier != editing || found->second.edited.sequence != expected)
        {
            throw std::runtime_error{"Конфликт"};
        }
        m_corrections.erase(found);
    }
    std::vector<msa::core::SportWorkoutRecord> workoutHistory() const override
    {
        std::vector<msa::core::SportWorkoutRecord> result;
        for (const auto& [identifier, record] : m_records)
        {
            static_cast<void>(identifier);
            result.push_back(record);
        }
        return result;
    }
    std::uint64_t checkpointProgram(const msa::core::SportProgramCheckpoint& value, std::uint64_t expected) override
    {
        if ((m_drafts.contains(value.program.identifier) &&
             m_drafts.at(value.program.identifier).sequence != expected) ||
            (!m_drafts.contains(value.program.identifier) && expected != 0))
        {
            throw std::runtime_error{"Конфликт"};
        }
        auto saved = value;
        saved.sequence = expected + 1;
        m_drafts[value.program.identifier] = saved;
        return saved.sequence;
    }
    std::vector<msa::core::SportProgramCheckpoint> programDrafts() const override
    {
        std::vector<msa::core::SportProgramCheckpoint> result;
        for (const auto& [identifier, value] : m_drafts)
        {
            static_cast<void>(identifier);
            result.push_back(value);
        }
        return result;
    }
    void saveProgram(const msa::core::SportProgramRevision& value, const std::optional<std::string>& expected) override
    {
        if ((m_programs.contains(value.identifier) &&
             (!expected || m_programs.at(value.identifier).revision != expected)) ||
            (!m_programs.contains(value.identifier) && expected))
        {
            throw std::runtime_error{"Конфликт"};
        }
        m_programs[value.identifier] = value;
    }
    std::vector<msa::core::SportProgramRevision> programs() const override
    {
        std::vector<msa::core::SportProgramRevision> result;
        for (const auto& [identifier, program] : m_programs)
        {
            static_cast<void>(identifier);
            result.push_back(program);
        }
        return result;
    }
    void savePreference(const std::string& identifier, msa::core::SportExercisePreference value) override
    {
        m_preferences[identifier] = value;
    }
    std::map<std::string, msa::core::SportExercisePreference> preferences() const override
    {
        return m_preferences;
    }
    void saveRestrictions(const std::string& text) override
    {
        m_restrictions = text;
    }
    std::string restrictions() const override
    {
        return m_restrictions;
    }

private:
    std::optional<msa::core::SportCheckpoint> m_active{};
    std::map<std::string, msa::core::SportCorrectionCheckpoint> m_corrections{};
    std::map<std::string, msa::core::SportWorkoutRecord> m_records{};
    std::map<std::string, msa::core::SportProgramRevision> m_programs{};
    std::map<std::string, msa::core::SportExercisePreference> m_preferences{};
    std::map<std::string, msa::core::SportProgramCheckpoint> m_drafts{};
    std::string m_restrictions{};
};
void executeFileSql(const QString& path, const QString& statement)
{
    const auto name = QUuid::createUuid().toString();
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        {
            QSqlQuery query{database};
            ASSERT_TRUE(query.exec(statement)) << query.lastError().text().toStdString();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}
QByteArray readFile(const QString& path)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly))
    {
        throw std::runtime_error{"Не удалось открыть тестовый файл"};
    }
    return file.readAll();
}
void repositoryScenario(msa::core::SportWorkoutRepository& store)
{
    const auto result = msa::test::sportResult();
    const msa::core::SportCheckpoint checkpoint{result.workout, {{"mass", "1,"}}, 0};
    const auto sequence = store.checkpoint(checkpoint, 0);
    ASSERT_TRUE(store.activeWorkout());
    EXPECT_EQ(store.activeWorkout()->editorText.at("mass"), "1,");
    store.finishWorkout(result, sequence);
    store.finishWorkout(result, sequence);
    EXPECT_FALSE(store.activeWorkout());
    ASSERT_EQ(store.workoutHistory().size(), 1U);
}
} // namespace
TEST(SqliteSportStoreTest, SamePublicContractWorksInMemoryAndOnDisk)
{
    InMemorySportStore memory;
    repositoryScenario(memory);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    msa::infra::SqliteSportStore disk{directory.filePath("sport.sqlite")};
    repositoryScenario(disk);
}
TEST(SqliteSportStoreTest, RestoresRawCheckpointAndIndependentPreferencesAfterReopen)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    {
        msa::infra::SqliteSportStore store{path};
        store.importExercises({msa::test::sportSession().items.front().exercise});
        store.savePreference("exercise", {true, false});
        store.saveRestrictions("Ограничение пользователя");
        const auto sequence = store.checkpoint({msa::test::sportDraft(), {{"repetitions", "1,"}}, 0}, 0);
        EXPECT_EQ(sequence, 1U);
        EXPECT_THROW(static_cast<void>(store.checkpoint({msa::test::sportDraft(), {{"repetitions", "2"}}, 0}, 0)),
                     std::runtime_error);
    }
    msa::infra::SqliteSportStore reopened{path};
    ASSERT_TRUE(reopened.activeWorkout());
    EXPECT_EQ(reopened.activeWorkout()->editorText.at("repetitions"), "1,");
    EXPECT_TRUE(reopened.preferences().at("exercise").favorite);
    EXPECT_FALSE(reopened.preferences().at("exercise").progression);
    EXPECT_EQ(reopened.restrictions(), "Ограничение пользователя");
}
TEST(SqliteSportStoreTest, CorrectionIsAtomicAndConflictDoesNotOverwrite)
{
    QTemporaryDir directory;
    msa::infra::SqliteSportStore store{directory.filePath("sport.sqlite")};
    const auto current = msa::test::sportResult();
    const auto sequence = store.checkpoint({current.workout, {}, 0}, 0);
    store.finishWorkout(current, sequence);
    auto edited = current.workout;
    edited.items.front().actual.front().repetitions = 12;
    const auto next = std::get<msa::core::SportWorkoutRevision>(
        msa::core::SportWorkoutService{}.correct(current, "revision", edited, "next"));
    store.correctWorkout(next, "revision");
    store.correctWorkout(next, "revision");
    auto stale = next;
    stale.identifier = "stale";
    EXPECT_THROW(store.correctWorkout(stale, "revision"), std::runtime_error);
    const auto history = store.workoutHistory();
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history.front().current.identifier, "next");
    const auto progress = std::get<std::vector<msa::core::SportProgressSeries>>(msa::core::buildSportProgress(history));
    EXPECT_EQ(progress.front().results.front().actual.repetitions, 12);
}
TEST(SqliteSportStoreTest, UnknownAndFutureSchemasAreByteForByteUnchanged)
{
    QTemporaryDir directory;
    const auto foreign = directory.filePath("foreign.sqlite");
    executeFileSql(foreign, "CREATE TABLE personal_data(value TEXT)");
    const auto bytes = readFile(foreign);
    EXPECT_THROW(msa::infra::SqliteSportStore{foreign}, std::runtime_error);
    EXPECT_EQ(readFile(foreign), bytes);
    const auto future = directory.filePath("future.sqlite");
    {
        msa::infra::SqliteSportStore store{future};
    }
    executeFileSql(future, "PRAGMA user_version=999");
    const auto futureBytes = readFile(future);
    EXPECT_THROW(msa::infra::SqliteSportStore{future}, std::runtime_error);
    EXPECT_EQ(readFile(future), futureBytes);
}
TEST(SqliteSportStoreTest, MigrationCreatesVerifiedBackupAndFailureKeepsOriginal)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    {
        msa::infra::SqliteSportStore store{path};
    }
    executeFileSql(path, "PRAGMA user_version=1");
    QString backup;
    {
        msa::infra::SqliteSportStore migrated{path, true};
        backup = migrated.lastBackupPath();
        EXPECT_FALSE(backup.isEmpty());
    }
    ASSERT_TRUE(QFileInfo::exists(backup));
    EXPECT_FALSE(readFile(backup).isEmpty());
    executeFileSql(path, "PRAGMA user_version=1");
    const auto blocked = directory.filePath("backups");
    // Отдельный путь проверяет отказ создания резервного каталога без удаления файлов.
    QTemporaryDir failureDirectory;
    const auto failurePath = failureDirectory.filePath("sport.sqlite");
    {
        msa::infra::SqliteSportStore store{failurePath};
    }
    executeFileSql(failurePath, "PRAGMA user_version=1");
    QFile obstacle{failureDirectory.filePath("backups")};
    ASSERT_TRUE(obstacle.open(QIODevice::WriteOnly));
    obstacle.close();
    const auto before = readFile(failurePath);
    EXPECT_THROW((msa::infra::SqliteSportStore{failurePath, true}), std::runtime_error);
    EXPECT_EQ(readFile(failurePath), before);
    static_cast<void>(blocked);
}
TEST(SqliteSportStoreTest, InvalidImportRollsBackAndDoesNotResetPreferences)
{
    QTemporaryDir directory;
    msa::infra::SqliteSportStore store{directory.filePath("sport.sqlite")};
    const auto exercise = msa::test::sportSession().items.front().exercise;
    store.importExercises({exercise});
    store.savePreference(exercise.identifier, {false, true});
    auto invalid = exercise;
    invalid.title = "Изменён тот же номер редакции";
    auto fresh = exercise;
    fresh.identifier = "fresh";
    EXPECT_THROW(store.importExercises({fresh, invalid}), std::runtime_error);
    EXPECT_EQ(store.exercises().size(), 1U);
    EXPECT_TRUE(store.preferences().at(exercise.identifier).progression);
}
TEST(SqliteSportStoreTest, CodecRoundTripsAbsenceAndRejectsBrokenVersion)
{
    auto draft = msa::test::sportDraft();
    const auto start = draft.timer.observed;
    draft.timer = std::get<msa::core::SportTimerState>(
        msa::core::registerSportAbsence(draft.timer, {"absence", start, start + std::chrono::seconds{20}}));
    draft.timer = std::get<msa::core::SportTimerState>(msa::core::resolveSportAbsence(
        draft.timer, "absence", msa::core::SportAbsenceDecision::Manual, std::chrono::seconds{5}));
    const auto bytes = msa::infra::SportDocumentCodec::encodeCheckpoint({draft, {{"mass", "1,"}}, 1});
    const auto restored = msa::infra::SportDocumentCodec::decodeCheckpoint(bytes);
    EXPECT_EQ(msa::core::summarizeSportDuration(restored.draft.timer).active, std::chrono::seconds{5});
    EXPECT_EQ(restored.editorText.at("mass"), "1,");
    auto invalid = bytes;
    invalid.replace("\"version\":1", "\"version\":99");
    EXPECT_THROW(static_cast<void>(msa::infra::SportDocumentCodec::decodeCheckpoint(invalid)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(msa::infra::SportDocumentCodec::decodeCheckpoint("{}")), std::runtime_error);
}

TEST(SqliteSportStoreTest, FailedWriteKeepsActiveCheckpointAndNoFalseHistory)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::infra::SqliteSportStore store{path};
    const auto result = msa::test::sportResult();
    const auto sequence = store.checkpoint({result.workout, {{"notes", "сохранённый ввод"}}, 0}, 0);
    executeFileSql(
        path,
        "CREATE TRIGGER reject_finish BEFORE INSERT ON workouts BEGIN SELECT RAISE(ABORT,'test write failure'); END");
    EXPECT_THROW(store.finishWorkout(result, sequence), std::runtime_error);
    EXPECT_TRUE(store.workoutHistory().empty());
    ASSERT_TRUE(store.activeWorkout());
    EXPECT_EQ(store.activeWorkout()->editorText.at("notes"), "сохранённый ввод");
}
TEST(SqliteSportStoreTest, BrokenCheckpointIsReportedWithoutReplacingFile)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::infra::SqliteSportStore store{path};
    static_cast<void>(store.checkpoint({msa::test::sportDraft(), {}, 0}, 0));
    executeFileSql(path, "UPDATE checkpoints SET payload='{}'");
    const auto bytes = readFile(path);
    EXPECT_THROW(static_cast<void>(store.activeWorkout()), std::runtime_error);
    EXPECT_EQ(readFile(path), bytes);
}
TEST(SqliteSportStoreTest, ProgramCompareAndSwapPreservesIndependentCopy)
{
    QTemporaryDir directory;
    msa::infra::SqliteSportStore store{directory.filePath("sport.sqlite")};
    msa::core::SportProgramRevision program{"program", "v1", "Тестовая программа", {}, {msa::test::sportSession()}};
    store.saveProgram(program, std::nullopt);
    auto copy = program;
    copy.identifier = "copy";
    copy.revision = "copy-v1";
    copy.sourceRevision = "v1";
    store.saveProgram(copy, std::nullopt);
    program.revision = "v2";
    program.title = "Новая редакция";
    store.saveProgram(program, std::string{"v1"});
    auto stale = program;
    stale.revision = "stale";
    EXPECT_THROW(store.saveProgram(stale, std::string{"v1"}), std::runtime_error);
    const auto programs = store.programs();
    ASSERT_EQ(programs.size(), 2U);
    EXPECT_EQ(programs.front().identifier, "copy");
    EXPECT_EQ(programs.front().title, "Тестовая программа");
}

TEST(SqliteSportStoreTest, IncompleteProgramDraftRestoresWithoutPretendingReady)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::core::SportProgramCheckpoint draft;
    draft.program.identifier = "own-program";
    draft.program.revision = "draft-version";
    draft.editorText = {{"repetitions", "1,"}};
    {
        msa::infra::SqliteSportStore store{path};
        EXPECT_EQ(store.checkpointProgram(draft, 0), 1U);
        EXPECT_EQ(store.checkpointProgram(draft, 0), 1U);
        EXPECT_TRUE(store.programs().empty());
    }
    msa::infra::SqliteSportStore reopened{path};
    ASSERT_EQ(reopened.programDrafts().size(), 1U);
    EXPECT_EQ(reopened.programDrafts().front().editorText.at("repetitions"), "1,");
    EXPECT_TRUE(msa::core::validateSportProgram(reopened.programDrafts().front().program));
}

TEST(SqliteSportStoreTest, SchemaThreeUpgradePreservesLegacyDocumentsMarksAndNewContentHead)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    auto legacy = msa::test::sportSession().items.front().exercise;
    legacy.loadKind = 1;
    legacy.revision = "legacy";
    const auto oldBytes = msa::infra::SportDocumentCodec::encodeExercise(legacy);
    EXPECT_FALSE(oldBytes.contains("dumbbellQuantity"));
    {
        msa::infra::SqliteSportStore store{path};
        store.importExercises({legacy});
        store.savePreference(legacy.identifier, {true, false});
        const auto result = msa::test::sportResult();
        EXPECT_FALSE(msa::infra::SportDocumentCodec::encodeRevision(result).contains("restrictionsSnapshot"));
        const auto sequence = store.checkpoint({result.workout, {}, 0}, 0);
        store.finishWorkout(result, sequence);
    }
    executeFileSql(path, "PRAGMA user_version=3");
    {
        msa::infra::SqliteSportStore store{path, true};
        ASSERT_FALSE(store.lastBackupPath().isEmpty());
        EXPECT_TRUE(QFileInfo::exists(store.lastBackupPath()));
        ASSERT_EQ(store.workoutHistory().size(), 1);
        EXPECT_TRUE(store.workoutHistory().front().current.workout.restrictionsSnapshot.empty());
        auto updated = legacy;
        updated.revision = "new";
        updated.requirements.dumbbellQuantity = 2;
        store.importExercises({updated});
        EXPECT_EQ(store.exercises().front().revision, "new");
        store.importExercises({legacy});
        EXPECT_EQ(store.exercises().front().revision, "new");
        EXPECT_TRUE(store.preferences().at(legacy.identifier).favorite);
        EXPECT_EQ(msa::infra::SportDocumentCodec::encodeExercise(legacy), oldBytes);
    }
}
TEST(SqliteSportStoreTest, RestrictionSnapshotRoundTripsInCheckpointAndCompletedRevision)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    auto result = msa::test::sportResult();
    result.workout.restrictionsSnapshot = "Индивидуальные условия на момент старта";
    {
        msa::infra::SqliteSportStore store{path};
        const auto sequence = store.checkpoint({result.workout, {}, 0}, 0);
        EXPECT_EQ(store.activeWorkout()->draft.restrictionsSnapshot, result.workout.restrictionsSnapshot);
        store.finishWorkout(result, sequence);
    }
    msa::infra::SqliteSportStore store{path};
    EXPECT_EQ(store.workoutHistory().front().current.workout.restrictionsSnapshot, result.workout.restrictionsSnapshot);
}

TEST(SqliteSportStoreTest, CorrectionCheckpointRetryAndBaselineAreStable)
{
    QTemporaryDir directory;
    msa::infra::SqliteSportStore store{directory.filePath("sport.sqlite")};
    const auto original = msa::test::sportResult();
    const auto sequence = store.checkpoint({original.workout, {}, 0}, 0);
    store.finishWorkout(original, sequence);
    msa::core::SportCorrectionCheckpoint draft{"editing-a", original, {original.workout, {{"notes", "1,"}}, 0}};
    draft.edited.draft.items.clear();
    EXPECT_EQ(store.checkpointCorrection(draft, 0), 1);
    EXPECT_EQ(store.checkpointCorrection(draft, 0), 1);
    auto restored = store.correctionDrafts().front();
    EXPECT_TRUE(restored.edited.draft.items.empty());
    EXPECT_EQ(restored.edited.editorText.at("notes"), "1,");
    draft.edited.editorText["notes"] = "different";
    EXPECT_THROW(store.checkpointCorrection(draft, 0), std::runtime_error);
    EXPECT_EQ(store.correctionDrafts().front().edited.editorText.at("notes"), "1,");
}
TEST(SqliteSportStoreTest, CorrectionIdentityPreventsAbaAndOldRetryDoesNotDeleteNewDraft)
{
    QTemporaryDir directory;
    msa::infra::SqliteSportStore store{directory.filePath("sport.sqlite")};
    const auto original = msa::test::sportResult();
    store.finishWorkout(original, store.checkpoint({original.workout, {}, 0}, 0));
    msa::core::SportCorrectionCheckpoint first{"editing-a", original, {original.workout, {}, 0}};
    EXPECT_EQ(store.checkpointCorrection(first, 0), 1);
    store.discardCorrection(original.workout.identifier, first.identifier, 1);
    auto second = first;
    second.identifier = "editing-b";
    EXPECT_EQ(store.checkpointCorrection(second, 0), 1);
    const auto revision = std::get<msa::core::SportWorkoutRevision>(
        msa::core::SportWorkoutService{}.correct(original, original.identifier, original.workout, "revision-new"));
    EXPECT_THROW(store.correctWorkout(revision, original.identifier, 1, first.identifier), std::runtime_error);
    EXPECT_THROW(store.discardCorrection(original.workout.identifier, first.identifier, 1), std::runtime_error);
    ASSERT_EQ(store.correctionDrafts().size(), 1);
    EXPECT_EQ(store.correctionDrafts().front().identifier, "editing-b");
    store.correctWorkout(revision, original.identifier, 1, second.identifier);
    EXPECT_TRUE(store.correctionDrafts().empty());
    msa::core::SportCorrectionCheckpoint third{"editing-c", revision, {revision.workout, {}, 0}};
    EXPECT_EQ(store.checkpointCorrection(third, 0), 1);
    store.correctWorkout(revision, original.identifier, 1, second.identifier);
    ASSERT_EQ(store.correctionDrafts().size(), 1);
    auto changed = store.correctionDrafts().front();
    changed.original = original;
    EXPECT_THROW(store.checkpointCorrection(changed, 1), std::runtime_error);
    EXPECT_EQ(store.correctionDrafts().front().original.identifier, revision.identifier);
}
TEST(SqliteSportStoreTest, FailedCorrectionCommitRetainsHeadDraftAndIndependentActiveWorkout)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::infra::SqliteSportStore store{path};
    const auto original = msa::test::sportResult();
    store.finishWorkout(original, store.checkpoint({original.workout, {}, 0}, 0));
    auto active = original.workout;
    active.identifier = "independent-active";
    static_cast<void>(store.checkpoint({active, {{"notes", "active"}}, 0}, 0));
    auto edited = original.workout;
    edited.notes = "Changed";
    const auto revision = std::get<msa::core::SportWorkoutRevision>(
        msa::core::SportWorkoutService{}.correct(original, original.identifier, edited, "corrected-result"));
    msa::core::SportCorrectionCheckpoint draft{"editing", original, {revision.workout, {{"notes", "Changed"}}, 0}};
    EXPECT_EQ(store.checkpointCorrection(draft, 0), 1);
    const auto before = msa::infra::SportDocumentCodec::encodeCorrectionCheckpoint(store.correctionDrafts().front());
    executeFileSql(
        path,
        "CREATE TRIGGER fail_correction BEFORE INSERT ON workout_revisions BEGIN SELECT RAISE(ABORT,'test'); END");
    EXPECT_THROW(store.correctWorkout(revision, original.identifier, 1, draft.identifier), std::runtime_error);
    EXPECT_EQ(store.workoutHistory().front().current.identifier, original.identifier);
    EXPECT_EQ(msa::infra::SportDocumentCodec::encodeCorrectionCheckpoint(store.correctionDrafts().front()), before);
    executeFileSql(path, "DROP TRIGGER fail_correction");
    store.correctWorkout(revision, original.identifier, 1, draft.identifier);
    EXPECT_TRUE(store.correctionDrafts().empty());
    EXPECT_EQ(store.activeWorkout()->draft.identifier, "independent-active");
    EXPECT_EQ(store.workoutHistory().size(), 1);
}
