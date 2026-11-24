#pragma once

#include "msa/core/SportExercise.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <map>

namespace msa::ui
{
/// @brief Офлайн-браузер карточек; файлы и SQL подключаются только связкой apps.
class SportLibraryModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(QString section READ section NOTIFY changed)
    Q_PROPERTY(bool showExercises READ showExercises NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QVariantList exercises READ exercises NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(int equipmentIndex READ equipmentIndex NOTIFY changed)
    Q_PROPERTY(int muscleIndex READ muscleIndex NOTIFY changed)
    Q_PROPERTY(QStringList equipmentOptions READ equipmentOptions CONSTANT)
    Q_PROPERTY(QStringList muscleOptions READ muscleOptions NOTIFY changed)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY changed)
    Q_PROPERTY(bool progressionOnly READ progressionOnly WRITE setProgressionOnly NOTIFY changed)
    Q_PROPERTY(bool noFloor READ noFloor WRITE setNoFloor NOTIFY changed)
    Q_PROPERTY(bool noPalmSupport READ noPalmSupport WRITE setNoPalmSupport NOTIFY changed)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool storageReady READ storageReady NOTIFY changed)
public:
    explicit SportLibraryModel(QObject* parent = nullptr);
    /// @brief Применяет каталог и только подтверждённые локальные предпочтения.
    void applyData(std::vector<core::SportExerciseRevision> catalog,
                   std::map<std::string, core::SportExercisePreference> preferences, bool storageReady);
    /// @brief Обновляет только доступные карточки, сохраняя последние подтверждённые отметки.
    void applyCatalog(std::vector<core::SportExerciseRevision> catalog);
    /// @brief Отмечает недоступность хранения без очистки последних данных.
    void reportStorageError(const QString& text);
    void applyPreference(const std::string& identifier, core::SportExercisePreference preference);
    void reportError(const QString& text);
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QString section() const;
    [[nodiscard]] bool showExercises() const;
    [[nodiscard]] QString summary() const;
    [[nodiscard]] QVariantList exercises() const;
    [[nodiscard]] QString query() const;
    [[nodiscard]] QStringList equipmentOptions() const;
    [[nodiscard]] int equipmentIndex() const;
    [[nodiscard]] int muscleIndex() const;
    [[nodiscard]] QStringList muscleOptions() const;
    [[nodiscard]] bool favoritesOnly() const;
    [[nodiscard]] bool progressionOnly() const;
    [[nodiscard]] bool noFloor() const;
    [[nodiscard]] bool noPalmSupport() const;
    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] QVariantMap selected() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] bool storageReady() const;
    void setQuery(const QString& value);
    void setFavoritesOnly(bool value);
    void setProgressionOnly(bool value);
    void setNoFloor(bool value);
    void setNoPalmSupport(bool value);
    Q_INVOKABLE void navigate(const QString& section);
    Q_INVOKABLE void openExercises();
    Q_INVOKABLE void openPrograms();
    Q_INVOKABLE void selectExercise(const QString& identifier);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setEquipmentFilter(const QString& value);
    Q_INVOKABLE void setMuscleFilter(const QString& value);
    Q_INVOKABLE void requestFavorite();
    Q_INVOKABLE void requestProgression();
    Q_INVOKABLE void resetFilters();
    Q_INVOKABLE void retry();
signals:
    void changed();
    void preferenceRequested(QString identifier, bool favorite, bool progression);
    void reloadRequested();

private:
    [[nodiscard]] const core::SportExerciseRevision* find(const std::string& identifier) const;
    [[nodiscard]] core::SportExercisePreference preference(const std::string& identifier) const;
    [[nodiscard]] bool matches(const core::SportExerciseRevision& exercise) const;

private:
    std::vector<core::SportExerciseRevision> m_catalog{};
    std::map<std::string, core::SportExercisePreference> m_preferences{};
    QString m_section{"overview"};
    bool m_showExercises{true};
    QString m_query{};
    QString m_equipment{};
    QString m_muscle{};
    QString m_selected{};
    QString m_error{};
    bool m_favoritesOnly{false};
    bool m_progressionOnly{false};
    bool m_noFloor{false};
    bool m_noPalmSupport{false};
    bool m_storageReady{false};
};
} // namespace msa::ui
