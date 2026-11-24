#pragma once

#include "msa/core/SportPreferencesRepository.h"
#include "msa/core/SportProgramRepository.h"
#include "msa/core/SportWorkoutRepository.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>
#include <stdexcept>

namespace msa::infra
{
/// @brief Существующая база требует явного разрешения миграции; файл не изменён.
class SportMigrationRequired final : public std::runtime_error
{
public:
    SportMigrationRequired();
};
/// @brief Отдельная клиентская база спорта; серверные схемы не открывает для записи.
class SqliteSportStore final : public core::SportWorkoutRepository,
                               public core::SportProgramRepository,
                               public core::SportPreferencesRepository
{
public:
    /// @brief Проверяет тип/версию до записи; миграции требуют проверенной резервной копии.
    explicit SqliteSportStore(const QString& path, bool allowMigration = false);
    ~SqliteSportStore() override;
    SqliteSportStore(const SqliteSportStore&) = delete;
    SqliteSportStore& operator=(const SqliteSportStore&) = delete;
    [[nodiscard]] QString lastBackupPath() const;
    /// @brief Импортирует проверенный пакет атомарно; существующие редакции неизменяемы.
    void importExercises(const std::vector<core::SportExerciseRevision>& exercises);
    [[nodiscard]] std::vector<core::SportExerciseRevision> exercises() const;
    [[nodiscard]] std::optional<core::SportCheckpoint> activeWorkout() const override;
    [[nodiscard]] std::uint64_t checkpoint(const core::SportCheckpoint& value, std::uint64_t expectedSequence) override;
    void finishWorkout(const core::SportWorkoutRevision& value, std::uint64_t expectedSequence) override;
    void correctWorkout(const core::SportWorkoutRevision& value, const std::string& expectedRevision,
                        std::optional<std::uint64_t> expectedDraftSequence = std::nullopt,
                        const std::optional<std::string>& expectedEditingIdentifier = std::nullopt) override;
    [[nodiscard]] std::uint64_t checkpointCorrection(const core::SportCorrectionCheckpoint& value,
                                                     std::uint64_t expectedSequence) override;
    [[nodiscard]] std::vector<core::SportCorrectionCheckpoint> correctionDrafts() const override;
    void discardCorrection(const std::string& workoutIdentifier, const std::string& editingIdentifier,
                           std::uint64_t expectedSequence) override;
    [[nodiscard]] std::vector<core::SportWorkoutRecord> workoutHistory() const override;
    [[nodiscard]] std::uint64_t checkpointProgram(const core::SportProgramCheckpoint& value,
                                                  std::uint64_t expectedSequence) override;
    [[nodiscard]] std::vector<core::SportProgramCheckpoint> programDrafts() const override;
    void saveProgram(const core::SportProgramRevision& value,
                     const std::optional<std::string>& expectedRevision) override;
    [[nodiscard]] std::vector<core::SportProgramRevision> programs() const override;
    void savePreference(const std::string& exerciseIdentifier, core::SportExercisePreference preference) override;
    [[nodiscard]] std::map<std::string, core::SportExercisePreference> preferences() const override;
    void saveRestrictions(const std::string& text) override;
    [[nodiscard]] std::string restrictions() const override;

private:
    void initialize(const QString& path, bool allowMigration);
    void transaction(const std::function<void()>& operation);
    void ensureUsable() const;
    void release();
    void execute(const QString& statement);

private:
    QString m_name{};
    QSqlDatabase m_database{};
    QString m_lastBackupPath{};
    bool m_failed{false};
};
} // namespace msa::infra
