#pragma once

#include "msa/ui/SportProgramEditor.h"
#include "msa/ui/SportWorkoutEditor.h"

#include <QObject>

namespace msa::ui
{
/// @brief Локальные списки программ/черновиков; команды связывает apps.
class SportJournalModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(msa::ui::SportWorkoutEditor* workoutEditor READ workoutEditor CONSTANT)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantList progress READ progress NOTIFY changed)
    Q_PROPERTY(QVariantList progressExercises READ progressExercises NOTIFY changed)
    Q_PROPERTY(QString progressExercise READ progressExercise WRITE setProgressExercise NOTIFY changed)
    Q_PROPERTY(QString restrictions READ restrictions WRITE setRestrictions NOTIFY restrictionsChanged)
    Q_PROPERTY(msa::ui::SportProgramEditor* programEditor READ programEditor CONSTANT)
    Q_PROPERTY(QVariantList programs READ programs NOTIFY changed)
    Q_PROPERTY(QVariantList drafts READ drafts NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
    explicit SportJournalModel(QObject* parent = nullptr);
    [[nodiscard]] SportProgramEditor* programEditor();
    [[nodiscard]] SportWorkoutEditor* workoutEditor();
    [[nodiscard]] QVariantList history() const;
    [[nodiscard]] QVariantList progress() const;
    [[nodiscard]] QVariantList progressExercises() const;
    [[nodiscard]] QString progressExercise() const;
    void setProgressExercise(const QString& identifier);
    [[nodiscard]] const std::vector<core::SportWorkoutRecord>& savedHistory() const;
    void applyHistory(std::vector<core::SportWorkoutRecord> history);
    [[nodiscard]] QString restrictions() const;
    void applyRestrictions(const QString& text);
    void setRestrictions(const QString& text);
    Q_INVOKABLE void saveRestrictions();
    Q_INVOKABLE void openWorkout(const QString& identifier);
    [[nodiscard]] QVariantList programs() const;
    [[nodiscard]] QVariantList drafts() const;
    [[nodiscard]] QString errorText() const;
    /// @brief Применяет только успешно прочитанные текущие программы и черновики.
    void applyPrograms(std::vector<core::SportProgramRevision> programs,
                       std::vector<core::SportProgramCheckpoint> drafts);
    [[nodiscard]] const std::vector<core::SportProgramRevision>& savedPrograms() const;
    [[nodiscard]] const std::vector<core::SportProgramCheckpoint>& savedDrafts() const;
    void reportError(const QString& message);
    Q_INVOKABLE void createProgram();
    Q_INVOKABLE void editProgram(const QString& identifier);
    Q_INVOKABLE void copyProgram(const QString& identifier);
    Q_INVOKABLE void resumeProgram(const QString& identifier);
    Q_INVOKABLE void startWorkout(const QString& identifier, int sessionIndex);
signals:
    void changed();
    void restrictionsChanged();
    void restrictionsRequested();
    void openWorkoutRequested(QString identifier);
    void createProgramRequested();
    void editProgramRequested(QString identifier);
    void copyProgramRequested(QString identifier);
    void resumeProgramRequested(QString identifier);
    void startWorkoutRequested(QString identifier, int sessionIndex);

private:
    SportProgramEditor m_programEditor{};
    SportWorkoutEditor m_workoutEditor{};
    std::vector<core::SportWorkoutRecord> m_history{};
    QString m_restrictions{};
    std::vector<core::SportProgramRevision> m_programs{};
    std::vector<core::SportProgramCheckpoint> m_drafts{};
    QString m_error{};
    QString m_progressExercise{};
};
} // namespace msa::ui
