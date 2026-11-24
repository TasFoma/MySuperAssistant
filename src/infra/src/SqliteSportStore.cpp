#include "msa/infra/SqliteSportStore.h"

#include "msa/core/SportWorkoutService.h"
#include "msa/infra/SportDocumentCodec.h"

#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <QVariant>
#include <limits>
#include <set>
#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr int kSportApplicationId = 0x4d535350;
// Версия 4 также ограждает новые поля документов от записи старым клиентом версии 3.
constexpr int kSportSchemaVersion = 5;
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}
[[nodiscard]] QSqlQuery query(const QSqlDatabase& database, const QString& sql, const QVariantList& values = {})
{
    QSqlQuery result{database};
    require(result.prepare(sql), "Не удалось подготовить локальную операцию спорта.");
    for (const auto& value : values)
    {
        result.addBindValue(value);
    }
    require(result.exec(), "Не удалось выполнить локальную операцию спорта.");

    return result;
}
[[nodiscard]] int scalar(const QSqlDatabase& database, const QString& sql)
{
    auto result = query(database, sql);
    require(result.next(), "Не удалось прочитать версию базы спорта.");

    return result.value(0).toInt();
}
[[nodiscard]] QString string(const std::string& text)
{
    return text.empty() ? QStringLiteral("") : QString::fromStdString(text);
}
[[nodiscard]] QByteArray revisionBytes(const QSqlDatabase& database, const QString& identifier)
{
    auto result = query(database, "SELECT payload FROM workout_revisions WHERE revision=?", {identifier});

    return result.next() ? result.value(0).toByteArray() : QByteArray{};
}
void verifyBackup(const QString& path, const QString& name, int version)
{
    auto backup = QSqlDatabase::addDatabase("QSQLITE", name);
    try
    {
        backup.setConnectOptions("QSQLITE_OPEN_READONLY");
        backup.setDatabaseName(path);
        require(backup.open(), "Резервная копия не открывается. Миграция отменена.");
        {
            auto check = query(backup, "PRAGMA quick_check");
            require(check.next() && check.value(0).toString() == "ok" &&
                        scalar(backup, "PRAGMA user_version") == version &&
                        scalar(backup, "PRAGMA application_id") == kSportApplicationId,
                    "Резервная копия не прошла проверку. Миграция отменена.");
        }
        backup.close();
    }
    catch (...)
    {
        backup.close();
        backup = QSqlDatabase{};
        QSqlDatabase::removeDatabase(name);
        throw;
    }
    backup = QSqlDatabase{};
    QSqlDatabase::removeDatabase(name);
}
} // namespace
SportMigrationRequired::SportMigrationRequired() :
    std::runtime_error{"База спорта требует обновления. Она не изменена. Обновление выполняется только с вашего "
                       "разрешения и с проверенной резервной копией."}
{
}
SqliteSportStore::SqliteSportStore(const QString& path, bool allowMigration)
{
    require(QSqlDatabase::isDriverAvailable("QSQLITE"), "Драйвер QSQLITE недоступен. Спортивные данные не открыты.");
    require(!path.trimmed().isEmpty(), "Не задан путь клиентской базы спорта.");
    m_name = "msa_sport_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_name);
    try
    {
        initialize(path, allowMigration);
    }
    catch (...)
    {
        release();
        throw;
    }
}
SqliteSportStore::~SqliteSportStore()
{
    release();
}
void SqliteSportStore::release()
{
    m_database.close();
    m_database = QSqlDatabase{};
    QSqlDatabase::removeDatabase(m_name);
}
void SqliteSportStore::ensureUsable() const
{
    require(!m_failed && m_database.isOpen() && m_database.thread() == QThread::currentThread(),
            "База спорта недоступна или используется из другого потока.");
}
void SqliteSportStore::execute(const QString& statement)
{
    ensureUsable();
    static_cast<void>(query(m_database, statement));
}
void SqliteSportStore::transaction(const std::function<void()>& operation)
{
    execute("BEGIN IMMEDIATE");
    try
    {
        operation();
        execute("COMMIT");
    }
    catch (...)
    {
        if (!m_database.rollback())
        {
            m_failed = true;
            m_database.close();
        }
        throw;
    }
}
void SqliteSportStore::initialize(const QString& path, bool allowMigration)
{
    const auto exists = QFileInfo::exists(path);
    int version{};
    m_database.setDatabaseName(path);
    if (exists)
    {
        m_database.setConnectOptions("QSQLITE_OPEN_READONLY");
        require(m_database.open(), "Не удалось прочитать существующую базу спорта.");
        require(scalar(m_database, "PRAGMA application_id") == kSportApplicationId,
                "Файл не является клиентской базой спорта. Он не изменён.");
        version = scalar(m_database, "PRAGMA user_version");
        require(version >= 1 && version <= kSportSchemaVersion,
                "Версия клиентской базы спорта не поддерживается. Файл не изменён.");
        {
            auto marker = query(m_database, "SELECT kind FROM sport_meta WHERE slot=1");
            require(marker.next() && marker.value(0).toString() == "sport-client", "Неизвестный тип базы спорта.");
        }
        if (version < kSportSchemaVersion)
        {
            if (!allowMigration)
            {
                throw SportMigrationRequired{};
            }
            const auto directory = QFileInfo{path}.absoluteDir().filePath("backups");
            require(QDir{}.mkpath(directory), "Не удалось создать каталог резервных копий. Миграция отменена.");
            m_lastBackupPath = QDir{directory}.filePath("sport-before-v" + QString::number(kSportSchemaVersion) + "-" +
                                                        QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
            static_cast<void>(query(m_database, "PRAGMA synchronous=FULL"));
            static_cast<void>(query(m_database, "VACUUM INTO ?", {m_lastBackupPath}));
            verifyBackup(m_lastBackupPath, m_name + "_backup", version);
        }
        m_database.close();
    }
    else
    {
        require(QDir{}.mkpath(QFileInfo{path}.absolutePath()), "Не удалось создать каталог данных спорта.");
    }
    m_database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    require(m_database.open(), "Не удалось открыть локальную базу спорта для записи.");
    execute("PRAGMA foreign_keys=ON");
    execute("PRAGMA synchronous=FULL");
    if (version == kSportSchemaVersion)
    {
        return;
    }
    transaction(
        [this, version]
        {
            if (version != 0)
            {
                require(scalar(m_database, "PRAGMA application_id") == kSportApplicationId &&
                            scalar(m_database, "PRAGMA user_version") == version,
                        "Тип или версия базы изменились до миграции. Обновление отменено.");
                auto marker = query(m_database, "SELECT kind FROM sport_meta WHERE slot=1");
                require(marker.next() && marker.value(0).toString() == "sport-client", "Изменился тип базы спорта.");
            }
            if (version == 0)
            {
                require(
                    scalar(m_database, "PRAGMA application_id") == 0 &&
                        scalar(m_database, "PRAGMA user_version") == 0 &&
                        scalar(m_database,
                               "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'") ==
                            0,
                    "По выбранному пути появилась другая база. Она не изменена.");
                execute("PRAGMA application_id=" + QString::number(kSportApplicationId));
                execute("CREATE TABLE sport_meta(slot INTEGER PRIMARY KEY CHECK(slot=1),kind TEXT NOT "
                        "NULL,restrictions TEXT NOT NULL DEFAULT '')");
                execute("INSERT INTO sport_meta(slot,kind) VALUES(1,'sport-client')");
                execute("CREATE TABLE exercise_revisions(id TEXT NOT NULL,revision TEXT NOT NULL,payload BLOB NOT "
                        "NULL,PRIMARY KEY(id,revision))");
                execute("CREATE TABLE exercises(id TEXT PRIMARY KEY,revision TEXT NOT NULL,FOREIGN KEY(id,revision) "
                        "REFERENCES exercise_revisions(id,revision))");
                execute(
                    "CREATE TABLE preferences(exercise_id TEXT PRIMARY KEY REFERENCES exercises(id),favorite INTEGER "
                    "NOT NULL CHECK(favorite IN(0,1)),progression INTEGER NOT NULL CHECK(progression IN(0,1)))");
                execute("CREATE TABLE checkpoints(slot INTEGER PRIMARY KEY CHECK(slot=1),workout_id TEXT NOT "
                        "NULL,sequence TEXT NOT NULL,payload BLOB NOT NULL)");
                execute("CREATE TABLE workouts(id TEXT PRIMARY KEY,revision TEXT NOT NULL,date TEXT NOT NULL)");
                execute("CREATE TABLE workout_revisions(revision TEXT PRIMARY KEY,workout_id TEXT NOT NULL REFERENCES "
                        "workouts(id),parent TEXT NOT NULL,payload BLOB NOT NULL)");
                execute("CREATE TABLE programs(id TEXT PRIMARY KEY,revision TEXT NOT NULL)");
                execute("CREATE TABLE program_revisions(revision TEXT PRIMARY KEY,program_id TEXT NOT NULL REFERENCES "
                        "programs(id),payload BLOB NOT NULL)");
            }
            execute("CREATE INDEX IF NOT EXISTS sport_workout_date ON workouts(date DESC,id)");
            execute("CREATE TABLE IF NOT EXISTS program_drafts(id TEXT PRIMARY KEY,sequence TEXT NOT NULL,payload BLOB "
                    "NOT NULL)");
            execute("CREATE TABLE IF NOT EXISTS correction_drafts(workout_id TEXT PRIMARY KEY REFERENCES "
                    "workouts(id),editing_id TEXT NOT NULL,sequence TEXT NOT NULL,payload BLOB NOT NULL)");
            execute("PRAGMA user_version=" + QString::number(kSportSchemaVersion));
        });
}
QString SqliteSportStore::lastBackupPath() const
{
    return m_lastBackupPath;
}
void SqliteSportStore::importExercises(const std::vector<core::SportExerciseRevision>& exercises)
{
    std::vector<QByteArray> documents;
    std::set<std::string> identifiers;
    for (const auto& exercise : exercises)
    {
        require(identifiers.insert(exercise.identifier).second, "Повторная идентичность в пакете упражнений.");
        documents.push_back(SportDocumentCodec::encodeExercise(exercise));
    }
    transaction(
        [this, &exercises, &documents]
        {
            std::size_t position{};
            for (const auto& exercise : exercises)
            {
                const auto& bytes = documents.at(position++);
                auto previous = query(m_database, "SELECT payload FROM exercise_revisions WHERE id=? AND revision=?",
                                      {string(exercise.identifier), string(exercise.revision)});
                const auto knownRevision = previous.next();
                if (knownRevision)
                {
                    require(previous.value(0).toByteArray() == bytes,
                            "Содержимое существующей редакции упражнения изменено.");
                }
                else
                {
                    static_cast<void>(query(m_database, "INSERT INTO exercise_revisions VALUES(?,?,?)",
                                            {string(exercise.identifier), string(exercise.revision), bytes}));
                }
                // Уже встречавшаяся старая редакция не откатывает голову каталога при повторном импорте.
                auto head =
                    query(m_database, "SELECT revision FROM exercises WHERE id=?", {string(exercise.identifier)});
                const auto hasHead = head.next();
                if (!hasHead || !knownRevision || head.value(0).toString() == string(exercise.revision))
                {
                    static_cast<void>(query(
                        m_database,
                        "INSERT INTO exercises VALUES(?,?) ON CONFLICT(id) DO UPDATE SET revision=excluded.revision",
                        {string(exercise.identifier), string(exercise.revision)}));
                }
            }
        });
}
std::vector<core::SportExerciseRevision> SqliteSportStore::exercises() const
{
    ensureUsable();
    auto rows = query(m_database, "SELECT r.payload FROM exercises e JOIN exercise_revisions r ON r.id=e.id AND "
                                  "r.revision=e.revision ORDER BY e.id");
    std::vector<core::SportExerciseRevision> result;
    while (rows.next())
    {
        result.push_back(SportDocumentCodec::decodeExercise(rows.value(0).toByteArray()));
    }

    return result;
}
std::optional<core::SportCheckpoint> SqliteSportStore::activeWorkout() const
{
    ensureUsable();
    auto row = query(m_database, "SELECT workout_id,sequence,payload FROM checkpoints WHERE slot=1");
    if (!row.next())
    {
        return std::nullopt;
    }
    const auto result = SportDocumentCodec::decodeCheckpoint(row.value(2).toByteArray());
    require(string(result.draft.identifier) == row.value(0).toString() &&
                QString::number(result.sequence) == row.value(1).toString(),
            "Метаданные черновика не совпадают с содержимым.");

    return result;
}
std::uint64_t SqliteSportStore::checkpoint(const core::SportCheckpoint& value, std::uint64_t expectedSequence)
{
    std::uint64_t sequence{};
    transaction(
        [this, &value, expectedSequence, &sequence]
        {
            const auto previous = activeWorkout();
            require(!previous || previous->draft.identifier == value.draft.identifier,
                    "На устройстве уже есть другая активная тренировка.");
            auto candidate = value;
            if (previous && previous->sequence != expectedSequence)
            {
                candidate.sequence = previous->sequence;
                require(expectedSequence < std::numeric_limits<std::uint64_t>::max() &&
                            expectedSequence + 1 == previous->sequence &&
                            SportDocumentCodec::encodeCheckpoint(candidate) ==
                                SportDocumentCodec::encodeCheckpoint(*previous),
                        "Черновик уже изменён.");
                sequence = previous->sequence;
                return;
            }
            require(previous || expectedSequence == 0, "Ожидаемый черновик не найден.");
            auto completed = query(m_database, "SELECT 1 FROM workouts WHERE id=?", {string(value.draft.identifier)});
            require(!completed.next(), "Тренировка уже завершена.");
            require(expectedSequence < std::numeric_limits<std::uint64_t>::max(), "Номер сохранения исчерпан.");
            candidate.sequence = expectedSequence + 1;
            const auto bytes = SportDocumentCodec::encodeCheckpoint(candidate);
            static_cast<void>(
                query(m_database,
                      "INSERT INTO checkpoints VALUES(1,?,?,?) ON CONFLICT(slot) DO UPDATE SET "
                      "workout_id=excluded.workout_id,sequence=excluded.sequence,payload=excluded.payload",
                      {string(candidate.draft.identifier), QString::number(candidate.sequence), bytes}));
            sequence = candidate.sequence;
        });

    return sequence;
}
void SqliteSportStore::finishWorkout(const core::SportWorkoutRevision& value, std::uint64_t expectedSequence)
{
    const auto bytes = SportDocumentCodec::encodeRevision(value);
    require(value.parentRevision.empty(), "Завершение создаёт первую редакцию.");
    transaction(
        [this, &value, expectedSequence, &bytes]
        {
            const auto previousBytes = revisionBytes(m_database, string(value.identifier));
            if (!previousBytes.isEmpty())
            {
                require(previousBytes == bytes, "Конфликт редакции результата.");
                return;
            }
            const auto checkpointValue = activeWorkout();
            require(checkpointValue && checkpointValue->draft.identifier == value.workout.identifier &&
                        checkpointValue->sequence == expectedSequence,
                    "Активная тренировка уже изменилась или не найдена.");
            auto candidate = *checkpointValue;
            candidate.draft = value.workout;
            require(SportDocumentCodec::encodeCheckpoint(candidate) ==
                        SportDocumentCodec::encodeCheckpoint(*checkpointValue),
                    "Сначала сохраните последние изменения активного занятия.");
            const auto date = QDate{static_cast<int>(value.workout.date.year()),
                                    static_cast<int>(static_cast<unsigned>(value.workout.date.month())),
                                    static_cast<int>(static_cast<unsigned>(value.workout.date.day()))}
                                  .toString(Qt::ISODate);
            static_cast<void>(query(m_database, "INSERT INTO workouts VALUES(?,?,?)",
                                    {string(value.workout.identifier), string(value.identifier), date}));
            static_cast<void>(
                query(m_database, "INSERT INTO workout_revisions VALUES(?,?,?,?)",
                      {string(value.identifier), string(value.workout.identifier), QStringLiteral(""), bytes}));
            execute("DELETE FROM checkpoints WHERE slot=1");
        });
}
void SqliteSportStore::correctWorkout(const core::SportWorkoutRevision& value, const std::string& expectedRevision,
                                      std::optional<std::uint64_t> expectedDraftSequence,
                                      const std::optional<std::string>& expectedEditingIdentifier)
{
    const auto bytes = SportDocumentCodec::encodeRevision(value);
    transaction(
        [this, &value, &expectedRevision, &bytes, expectedDraftSequence, &expectedEditingIdentifier]
        {
            const auto previousBytes = revisionBytes(m_database, string(value.identifier));
            if (!previousBytes.isEmpty())
            {
                require(previousBytes == bytes, "Конфликт идентичности редакции.");
                return;
            }
            if (expectedDraftSequence)
            {
                auto draft = query(m_database, "SELECT payload FROM correction_drafts WHERE workout_id=?",
                                   {string(value.workout.identifier)});
                require(draft.next(), "Черновик исправления не найден.");
                const auto saved = SportDocumentCodec::decodeCorrectionCheckpoint(draft.value(0).toByteArray());
                require(expectedEditingIdentifier && saved.identifier == *expectedEditingIdentifier &&
                            saved.edited.sequence == *expectedDraftSequence &&
                            saved.original.identifier == expectedRevision,
                        "Черновик исправления уже изменён.");
                auto candidate = saved;
                candidate.edited.draft = value.workout;
                require(SportDocumentCodec::encodeCorrectionCheckpoint(candidate) ==
                            SportDocumentCodec::encodeCorrectionCheckpoint(saved),
                        "Сначала сохраните последние изменения исправления.");
            }
            auto head =
                query(m_database, "SELECT revision FROM workouts WHERE id=?", {string(value.workout.identifier)});
            require(head.next() && head.value(0).toString() == string(expectedRevision),
                    "Результат уже изменён. Нужна сверка редакций.");
            require(value.parentRevision == expectedRevision, "Некорректная исходная редакция исправления.");
            const auto current =
                SportDocumentCodec::decodeRevision(revisionBytes(m_database, string(expectedRevision)));
            const auto checked =
                core::SportWorkoutService{}.correct(current, expectedRevision, value.workout, value.identifier);
            require(std::holds_alternative<core::SportWorkoutRevision>(checked) &&
                        SportDocumentCodec::encodeRevision(std::get<core::SportWorkoutRevision>(checked)) == bytes,
                    "Исправление меняет исходный план или временные отметки.");
            static_cast<void>(
                query(m_database, "INSERT INTO workout_revisions VALUES(?,?,?,?)",
                      {string(value.identifier), string(value.workout.identifier), string(expectedRevision), bytes}));
            const auto date = QDate{static_cast<int>(value.workout.date.year()),
                                    static_cast<int>(static_cast<unsigned>(value.workout.date.month())),
                                    static_cast<int>(static_cast<unsigned>(value.workout.date.day()))}
                                  .toString(Qt::ISODate);
            static_cast<void>(query(m_database, "UPDATE workouts SET revision=?,date=? WHERE id=?",
                                    {string(value.identifier), date, string(value.workout.identifier)}));
            if (expectedDraftSequence)
            {
                static_cast<void>(query(m_database, "DELETE FROM correction_drafts WHERE workout_id=?",
                                        {string(value.workout.identifier)}));
            }
        });
}
std::vector<core::SportWorkoutRecord> SqliteSportStore::workoutHistory() const
{
    ensureUsable();
    auto rows = query(
        m_database,
        "SELECT r.payload FROM workouts w JOIN workout_revisions r ON w.revision=r.revision ORDER BY w.date DESC,w.id");
    std::vector<core::SportWorkoutRecord> result;
    while (rows.next())
    {
        result.push_back({SportDocumentCodec::decodeRevision(rows.value(0).toByteArray())});
    }

    return result;
}
void SqliteSportStore::saveProgram(const core::SportProgramRevision& value,
                                   const std::optional<std::string>& expectedRevision)
{
    const auto bytes = SportDocumentCodec::encodeProgram(value);
    transaction(
        [this, &value, &expectedRevision, &bytes]
        {
            auto existing =
                query(m_database, "SELECT payload FROM program_revisions WHERE revision=?", {string(value.revision)});
            if (existing.next())
            {
                require(existing.value(0).toByteArray() == bytes, "Конфликт редакции программы.");
                return;
            }
            auto head = query(m_database, "SELECT revision FROM programs WHERE id=?", {string(value.identifier)});
            const auto found = head.next();
            require((!found && !expectedRevision) ||
                        (found && expectedRevision && head.value(0).toString() == string(*expectedRevision)),
                    "Программа уже изменилась.");
            static_cast<void>(query(
                m_database, "INSERT INTO programs VALUES(?,?) ON CONFLICT(id) DO UPDATE SET revision=excluded.revision",
                {string(value.identifier), string(value.revision)}));
            static_cast<void>(query(m_database, "INSERT INTO program_revisions VALUES(?,?,?)",
                                    {string(value.revision), string(value.identifier), bytes}));
        });
}
std::vector<core::SportProgramRevision> SqliteSportStore::programs() const
{
    ensureUsable();
    auto rows = query(
        m_database, "SELECT r.payload FROM programs p JOIN program_revisions r ON r.revision=p.revision ORDER BY p.id");
    std::vector<core::SportProgramRevision> result;
    while (rows.next())
    {
        result.push_back(SportDocumentCodec::decodeProgram(rows.value(0).toByteArray()));
    }

    return result;
}
void SqliteSportStore::savePreference(const std::string& exerciseIdentifier, core::SportExercisePreference preference)
{
    transaction(
        [this, &exerciseIdentifier, preference]
        {
            static_cast<void>(
                query(m_database,
                      "INSERT INTO preferences VALUES(?,?,?) ON CONFLICT(exercise_id) DO UPDATE SET "
                      "favorite=excluded.favorite,progression=excluded.progression",
                      {string(exerciseIdentifier), preference.favorite ? 1 : 0, preference.progression ? 1 : 0}));
        });
}
std::map<std::string, core::SportExercisePreference> SqliteSportStore::preferences() const
{
    ensureUsable();
    auto rows = query(m_database, "SELECT exercise_id,favorite,progression FROM preferences");
    std::map<std::string, core::SportExercisePreference> result;
    while (rows.next())
    {
        result.emplace(rows.value(0).toString().toStdString(),
                       core::SportExercisePreference{rows.value(1).toBool(), rows.value(2).toBool()});
    }

    return result;
}
void SqliteSportStore::saveRestrictions(const std::string& text)
{
    transaction(
        [this, &text]
        { static_cast<void>(query(m_database, "UPDATE sport_meta SET restrictions=? WHERE slot=1", {string(text)})); });
}
std::string SqliteSportStore::restrictions() const
{
    ensureUsable();
    auto row = query(m_database, "SELECT restrictions FROM sport_meta WHERE slot=1");
    require(row.next(), "Не найдены параметры спорта.");

    return row.value(0).toString().toStdString();
}
std::uint64_t SqliteSportStore::checkpointProgram(const core::SportProgramCheckpoint& value,
                                                  std::uint64_t expectedSequence)
{
    std::uint64_t sequence{};
    transaction(
        [this, &value, expectedSequence, &sequence]
        {
            auto candidate = value;
            auto previous = query(m_database, "SELECT sequence,payload FROM program_drafts WHERE id=?",
                                  {string(value.program.identifier)});
            const auto exists = previous.next();
            const auto current = exists ? SportDocumentCodec::decodeProgramCheckpoint(previous.value(1).toByteArray())
                                        : core::SportProgramCheckpoint{};
            require(!exists || QString::number(current.sequence) == previous.value(0).toString(),
                    "Повреждены метаданные черновика программы.");
            if (exists && current.sequence != expectedSequence)
            {
                candidate.sequence = current.sequence;
                require(expectedSequence < std::numeric_limits<std::uint64_t>::max() &&
                            expectedSequence + 1 == current.sequence &&
                            SportDocumentCodec::encodeProgramCheckpoint(candidate) ==
                                SportDocumentCodec::encodeProgramCheckpoint(current),
                        "Черновик программы уже изменился.");
                sequence = current.sequence;
                return;
            }
            require((exists || expectedSequence == 0) && expectedSequence < std::numeric_limits<std::uint64_t>::max(),
                    "Некорректный номер черновика программы.");
            candidate.sequence = expectedSequence + 1;
            const auto bytes = SportDocumentCodec::encodeProgramCheckpoint(candidate);
            static_cast<void>(query(m_database,
                                    "INSERT INTO program_drafts VALUES(?,?,?) ON CONFLICT(id) DO UPDATE SET "
                                    "sequence=excluded.sequence,payload=excluded.payload",
                                    {string(value.program.identifier), QString::number(candidate.sequence), bytes}));
            sequence = candidate.sequence;
        });

    return sequence;
}
std::vector<core::SportProgramCheckpoint> SqliteSportStore::programDrafts() const
{
    ensureUsable();
    auto rows = query(m_database, "SELECT id,sequence,payload FROM program_drafts ORDER BY id");
    std::vector<core::SportProgramCheckpoint> result;
    while (rows.next())
    {
        const auto value = SportDocumentCodec::decodeProgramCheckpoint(rows.value(2).toByteArray());
        require(string(value.program.identifier) == rows.value(0).toString() &&
                    QString::number(value.sequence) == rows.value(1).toString(),
                "Повреждены метаданные черновика программы.");
        result.push_back(value);
    }

    return result;
}

std::uint64_t SqliteSportStore::checkpointCorrection(const core::SportCorrectionCheckpoint& value,
                                                     std::uint64_t expected)
{
    std::uint64_t sequence{};
    transaction(
        [this, &value, expected, &sequence]
        {
            const auto originalBytes = revisionBytes(m_database, string(value.original.identifier));
            require(!originalBytes.isEmpty() &&
                        SportDocumentCodec::encodeRevision(SportDocumentCodec::decodeRevision(originalBytes)) ==
                            SportDocumentCodec::encodeRevision(value.original),
                    "Исходная редакция исправления не совпадает с историей.");
            auto previous =
                query(m_database, "SELECT editing_id,sequence,payload FROM correction_drafts WHERE workout_id=?",
                      {string(value.original.workout.identifier)});
            const auto exists = previous.next();
            auto candidate = value;
            const auto current = exists
                                     ? SportDocumentCodec::decodeCorrectionCheckpoint(previous.value(2).toByteArray())
                                     : core::SportCorrectionCheckpoint{};
            require(!exists || (current.identifier == previous.value(0).toString().toStdString() &&
                                QString::number(current.edited.sequence) == previous.value(1).toString()),
                    "Повреждены метаданные исправления.");
            require(!exists || (current.original.workout.identifier == value.original.workout.identifier &&
                                SportDocumentCodec::encodeRevision(current.original) ==
                                    SportDocumentCodec::encodeRevision(value.original)),
                    "Исходная редакция черновика исправления не может измениться.");
            require(!exists || current.identifier == value.identifier,
                    "Другое исправление уже открыто. Сначала откройте сохранённый черновик.");
            if (exists && current.edited.sequence != expected)
            {
                candidate.edited.sequence = current.edited.sequence;
                require(expected < std::numeric_limits<std::uint64_t>::max() &&
                            expected + 1 == current.edited.sequence &&
                            SportDocumentCodec::encodeCorrectionCheckpoint(candidate) ==
                                SportDocumentCodec::encodeCorrectionCheckpoint(current),
                        "Черновик исправления уже изменён.");
                sequence = current.edited.sequence;
                return;
            }
            require((exists || expected == 0) && expected < std::numeric_limits<std::uint64_t>::max(),
                    "Некорректный номер исправления.");
            candidate.edited.sequence = expected + 1;
            const auto bytes = SportDocumentCodec::encodeCorrectionCheckpoint(candidate);
            static_cast<void>(
                query(m_database,
                      "INSERT INTO correction_drafts VALUES(?,?,?,?) ON CONFLICT(workout_id) DO UPDATE SET "
                      "editing_id=excluded.editing_id,sequence=excluded.sequence,payload=excluded.payload",
                      {string(value.original.workout.identifier), string(value.identifier),
                       QString::number(candidate.edited.sequence), bytes}));
            sequence = candidate.edited.sequence;
        });
    return sequence;
}
std::vector<core::SportCorrectionCheckpoint> SqliteSportStore::correctionDrafts() const
{
    ensureUsable();
    auto rows =
        query(m_database, "SELECT workout_id,editing_id,sequence,payload FROM correction_drafts ORDER BY workout_id");
    std::vector<core::SportCorrectionCheckpoint> result;
    while (rows.next())
    {
        const auto value = SportDocumentCodec::decodeCorrectionCheckpoint(rows.value(3).toByteArray());
        require(string(value.original.workout.identifier) == rows.value(0).toString() &&
                    string(value.identifier) == rows.value(1).toString() &&
                    QString::number(value.edited.sequence) == rows.value(2).toString(),
                "Повреждены метаданные исправления.");
        result.push_back(value);
    }
    return result;
}
void SqliteSportStore::discardCorrection(const std::string& workout, const std::string& editing, std::uint64_t expected)
{
    transaction(
        [this, &workout, &editing, expected]
        {
            auto row = query(m_database, "SELECT editing_id,sequence FROM correction_drafts WHERE workout_id=?",
                             {string(workout)});
            if (!row.next())
            {
                return;
            }
            require(row.value(0).toString() == string(editing) && row.value(1).toString() == QString::number(expected),
                    "Черновик исправления уже изменён. Отмена не выполнена.");
            static_cast<void>(query(m_database, "DELETE FROM correction_drafts WHERE workout_id=?", {string(workout)}));
        });
}
} // namespace msa::infra
