#include "msa/infra/SqliteDatabase.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <vector>

namespace msa::infra {
namespace {

[[nodiscard]] std::vector<SqliteMigration> entryMigrations()
{
    return {{.version = 2, .statements = {QStringLiteral(
        "CREATE TABLE entries (identifier INTEGER PRIMARY KEY, value TEXT NOT NULL UNIQUE)")}}};
}

[[nodiscard]] QVariant scalar(const SqliteDatabase& database, const QString& statement)
{
    QSqlQuery query{database.connection()};
    if (!query.exec(statement) || !query.next()) {
        throw std::runtime_error{"Не удалось выполнить проверочный запрос"};
    }

    return query.value(0);
}

// Независимое соединение позволяет проверять отказ открытия неизвестных схем.
[[nodiscard]] QVariant rawStatement(const QString& path, const QString& statement)
{
    const auto name = QStringLiteral("msa_raw_test_") + QUuid::createUuid().toString();
    QVariant value;
    bool succeeded = false;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query{database};
            succeeded = query.exec(statement);
            if (succeeded && query.isSelect() && query.next()) {
                value = query.value(0);
            }
        }
    }
    QSqlDatabase::removeDatabase(name);
    if (!succeeded) {
        throw std::runtime_error{"Не удалось подготовить или прочитать тестовую базу"};
    }

    return value;
}

class SqliteDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override;
    [[nodiscard]] QString path() const;

private:
    QTemporaryDir m_directory{QStringLiteral("msa-sqlite-test-XXXXXX")};
};

void SqliteDatabaseTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
}

QString SqliteDatabaseTest::path() const
{
    return m_directory.filePath(QStringLiteral("пустая база.sqlite"));
}

TEST_F(SqliteDatabaseTest, CreatesEmptyBaselineAndReopensWithSameIdentityAndNoDomainTables)
{
    int identity = 0;
    {
        const SqliteDatabase database{path()};
        EXPECT_EQ(database.schemaVersion(), 1);
        identity = scalar(database, QStringLiteral("PRAGMA application_id")).toInt();
        EXPECT_NE(identity, 0);
        EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM sqlite_schema")).toInt(), 0);
        EXPECT_EQ(scalar(database, QStringLiteral("PRAGMA foreign_keys")).toInt(), 1);
    }
    const SqliteDatabase reopened{path()};
    EXPECT_EQ(reopened.schemaVersion(), 1);
    EXPECT_EQ(scalar(reopened, QStringLiteral("PRAGMA application_id")).toInt(), identity);
}

TEST_F(SqliteDatabaseTest, AppliesSequentialMigrationsOnceAndKeepsFullTextAcrossReopen)
{
    auto migrations = entryMigrations();
    migrations.push_back(SqliteMigration{.version = 3, .statements = {
        QStringLiteral("ALTER TABLE entries ADD COLUMN detail TEXT"),
        QStringLiteral("INSERT INTO entries(value, detail) VALUES ('начало', 'полный текст🙂')")}});
    {
        const SqliteDatabase database{path(), migrations};
        EXPECT_EQ(database.schemaVersion(), 3);
        EXPECT_EQ(scalar(database, QStringLiteral("SELECT detail FROM entries")).toString(),
            QStringLiteral("полный текст🙂"));
    }
    const SqliteDatabase reopened{path(), migrations};
    EXPECT_EQ(reopened.schemaVersion(), 3);
    EXPECT_EQ(scalar(reopened, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 1);
}

TEST_F(SqliteDatabaseTest, FailedUpgradeRollsBackEarlierMigrationDdlDataAndVersion)
{
    {
        const SqliteDatabase database{path()};
        EXPECT_EQ(database.schemaVersion(), 1);
    }
    auto migrations = entryMigrations();
    migrations.push_back(SqliteMigration{.version = 3, .statements = {
        QStringLiteral("INSERT INTO entries(value) VALUES ('не сохранять')"),
        QStringLiteral("INSERT INTO missing_table(value) VALUES ('ошибка')")}});
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path(), migrations}), std::runtime_error);
    const SqliteDatabase reopened{path()};
    EXPECT_EQ(reopened.schemaVersion(), 1);
    EXPECT_EQ(scalar(reopened, QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_schema WHERE name = 'entries'")).toInt(), 0);
}

TEST_F(SqliteDatabaseTest, FailedInitializationDoesNotLeaveBaselineIdentityOrVersion)
{
    const std::vector<SqliteMigration> broken{{.version = 2,
        .statements = {QStringLiteral("CREATE TABLE partial_table(value TEXT)"),
            QStringLiteral("INVALID SQL")}}};
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path(), broken}), std::runtime_error);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA user_version")).toInt(), 0);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA application_id")).toInt(), 0);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("SELECT COUNT(*) FROM sqlite_schema")).toInt(), 0);
}

TEST_F(SqliteDatabaseTest, RejectsNewerVersionWithoutChangingStoredVersionOrData)
{
    {
        SqliteDatabase database{path(), entryMigrations()};
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('сохранить')"));
        database.execute(QStringLiteral("PRAGMA user_version = 99"));
    }
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path(), entryMigrations()}), std::runtime_error);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA user_version")).toInt(), 99);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("SELECT value FROM entries")).toString(), QStringLiteral("сохранить"));
}

TEST_F(SqliteDatabaseTest, RejectsForeignUnversionedSchemaWithoutStampingOrDeletingData)
{
    static_cast<void>(rawStatement(path(), QStringLiteral("CREATE TABLE foreign_data(value TEXT)")));
    static_cast<void>(rawStatement(path(), QStringLiteral("INSERT INTO foreign_data VALUES ('старые данные')")));
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path()}), std::runtime_error);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA user_version")).toInt(), 0);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA application_id")).toInt(), 0);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("SELECT value FROM foreign_data")).toString(),
        QStringLiteral("старые данные"));
}

TEST_F(SqliteDatabaseTest, RejectsVersionedFileWithWrongApplicationIdentity)
{
    {
        SqliteDatabase database{path()};
        database.execute(QStringLiteral("PRAGMA application_id = 77"));
    }
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path()}), std::runtime_error);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA application_id")).toInt(), 77);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("PRAGMA user_version")).toInt(), 1);
}

TEST_F(SqliteDatabaseTest, CommitsGroupAndRollsBackOnDomainOrSqlErrorThenRemainsUsable)
{
    SqliteDatabase database{path(), entryMigrations()};
    database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('первый')"));
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('второй')"));
    });
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 2);
    EXPECT_THROW(database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('откат домена')"));
        throw std::logic_error{"Отказ предметной операции"};
    }), std::logic_error);
    EXPECT_THROW(database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('откат SQL')"));
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('первый')"));
    }), std::runtime_error);
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 2);
    database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('после ошибок')"));
    });
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 3);
}

TEST_F(SqliteDatabaseTest, NestedSuccessIsRolledBackWithOuterFailure)
{
    SqliteDatabase database{path(), entryMigrations()};
    EXPECT_THROW(database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('внешняя')"));
        database.transaction([&database]() {
            database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('внутренняя')"));
        });
        throw std::logic_error{"Отмена общей операции"};
    }), std::logic_error);
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 0);
}

TEST_F(SqliteDatabaseTest, CaughtInnerFailureRollsBackOnlyInnerWork)
{
    SqliteDatabase database{path(), entryMigrations()};
    database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('до')"));
        try {
            database.transaction([&database]() {
                database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('внутри')"));
                throw std::logic_error{"Отмена вложенной операции"};
            });
        } catch (const std::logic_error&) {
            database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('после')"));
        }
    });
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 2);
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM entries WHERE value = 'внутри'")).toInt(), 0);
}

TEST_F(SqliteDatabaseTest, DeferredForeignKeyFailureAtCommitRollsBackAllChanges)
{
    SqliteDatabase database{path()};
    database.execute(QStringLiteral("CREATE TABLE parent(identifier INTEGER PRIMARY KEY)"));
    database.execute(QStringLiteral("CREATE TABLE child(parent_id INTEGER REFERENCES parent(identifier) "
        "DEFERRABLE INITIALLY DEFERRED)"));
    EXPECT_THROW(database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO child(parent_id) VALUES (42)"));
    }), std::runtime_error);
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM child")).toInt(), 0);
    database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO parent VALUES (42)"));
        database.execute(QStringLiteral("INSERT INTO child VALUES (42)"));
    });
    EXPECT_EQ(scalar(database, QStringLiteral("SELECT COUNT(*) FROM child")).toInt(), 1);
}

TEST_F(SqliteDatabaseTest, FailedRollbackClosesConnectionInsteadOfAllowingFurtherWrites)
{
    SqliteDatabase database{path(), entryMigrations()};
    EXPECT_THROW(database.transaction([&database]() {
        database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('откатить')"));
        // SQLite отменяет всю транзакцию, поэтому точка отката уже исчезла.
        database.execute(QStringLiteral("INSERT OR ROLLBACK INTO entries(value) VALUES ('откатить')"));
    }), std::runtime_error);
    EXPECT_THROW(database.execute(QStringLiteral("INSERT INTO entries(value) VALUES ('запрещено')")), std::runtime_error);
    EXPECT_EQ(rawStatement(path(), QStringLiteral("SELECT COUNT(*) FROM entries")).toInt(), 0);
}

TEST_F(SqliteDatabaseTest, RejectsEmptyPathInvalidMigrationOrderAndEmptyOperation)
{
    EXPECT_THROW(static_cast<void>(SqliteDatabase{QString{}}), std::invalid_argument);
    const std::vector<SqliteMigration> gap{{.version = 3, .statements = {QStringLiteral("SELECT 1")}}};
    const std::vector<SqliteMigration> duplicate{{.version = 2}, {.version = 2}};
    const std::vector<SqliteMigration> blank{{.version = 2, .statements = {QStringLiteral("  ")}}};
    for (const auto& migrations : {gap, duplicate, blank}) {
        EXPECT_THROW(static_cast<void>(SqliteDatabase{path(), migrations}), std::invalid_argument);
    }
    SqliteDatabase database{path()};
    EXPECT_THROW(database.transaction(std::function<void()>{}), std::invalid_argument);
    EXPECT_EQ(database.schemaVersion(), 1);
}

TEST_F(SqliteDatabaseTest, IndependentConnectionsDoNotUseDefaultAndAreRemovedOnDestruction)
{
    const auto before = QSqlDatabase::connectionNames();
    QString connectionName;
    {
        const SqliteDatabase first{path()};
        const SqliteDatabase second{path()};
        connectionName = first.connection().connectionName();
        EXPECT_NE(connectionName, second.connection().connectionName());
        EXPECT_TRUE(QSqlDatabase::contains(connectionName));
        EXPECT_EQ(first.schemaVersion(), second.schemaVersion());
    }
    EXPECT_FALSE(QSqlDatabase::contains(connectionName));
    EXPECT_EQ(QSqlDatabase::connectionNames(), before);
    EXPECT_THROW(static_cast<void>(SqliteDatabase{path() + QStringLiteral("/missing/file.sqlite")}), std::runtime_error);
    EXPECT_EQ(QSqlDatabase::connectionNames(), before);
}

}  // namespace
}  // namespace msa::infra

