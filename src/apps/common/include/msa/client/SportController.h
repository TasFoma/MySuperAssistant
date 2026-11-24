#pragma once

#include "msa/infra/SqliteSportStore.h"
#include "msa/ui/SportLibraryModel.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <memory>

namespace msa::client
{
/// @brief Связывает офлайн-каталог и подтверждённые предпочтения с UI без TCP.
class SportController final : public QObject
{
public:
    /// @brief Конструктор не открывает пользовательские файлы.
    SportController(ui::SportLibraryModel& model, QString databasePath, QObject* parent = nullptr);
    /// @brief Загружает библиотеку и отдельную локальную базу; отказ виден в UI.
    void start();
    /// @brief Фиксирует уход/возврат приложения; интервал отсутствия требует выбора пользователя.
    void suspendWorkout();
    void resumeWorkout();

private:
    static std::string sportId();
    void connectWorkout();
    void initializeWorkout();
    void refreshHistory();
    void onStartWorkout(const QString& identifier, int sessionIndex);
    void onOpenWorkout(const QString& identifier);
    void onWorkoutCheckpoint();
    void onWorkoutFinish();
    void onWorkoutPause();
    void onWorkoutAbsence(const QString& identifier, const QString& decision);
    void onWorkoutReplacement(const QString& identifier, const QString& exerciseIdentifier);
    void observeWorkout();
    void refreshPrograms();
    void onCreateProgram();
    void onEditProgram(const QString& identifier, bool copy);
    void onResumeProgram(const QString& identifier);
    void onProgramCheckpoint();
    void onProgramSave();
    void onAddExercise(const QString& identifier);
    void onPreferenceRequested(const QString& identifier, bool favorite, bool progression);

private:
    ui::SportLibraryModel& m_model;
    QString m_databasePath{};
    std::unique_ptr<infra::SqliteSportStore> m_store{};
    std::vector<core::SportExerciseRevision> m_catalog{};
    QTimer m_workoutTimer{};
    QElapsedTimer m_elapsed{};
    bool m_absent{false};
    bool m_workoutInitialized{false};
};
} // namespace msa::client
