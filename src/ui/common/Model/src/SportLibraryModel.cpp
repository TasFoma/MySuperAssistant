#include "msa/ui/SportLibraryModel.h"

#include <algorithm>
#include <set>

namespace msa::ui
{
namespace
{
[[nodiscard]] QString string(const std::string& text)
{
    return QString::fromStdString(text);
}
[[nodiscard]] QString normalized(QString text)
{
    return text.toCaseFolded().replace(QChar{0x451}, QChar{0x435}).simplified();
}
[[nodiscard]] QString joined(const std::vector<std::string>& values, const QString& separator = ", ")
{
    QStringList result;
    for (const auto& value : values)
    {
        result.append(string(value));
    }

    return result.join(separator);
}
} // namespace
SportLibraryModel::SportLibraryModel(QObject* parent) : QObject{parent} {}
SportJournalModel* SportLibraryModel::journal() { return &m_journal; }
void SportLibraryModel::applyData(std::vector<core::SportExerciseRevision> catalog,
                                  std::map<std::string, core::SportExercisePreference> preferences, bool ready)
{
    m_catalog = std::move(catalog);
    m_preferences = std::move(preferences);
    m_storageReady = ready;
    m_error.clear();
    emit changed();
}
void SportLibraryModel::applyCatalog(std::vector<core::SportExerciseRevision> catalog)
{
    m_catalog = std::move(catalog);
    emit changed();
}
void SportLibraryModel::reportStorageError(const QString& text)
{
    m_storageReady = false;
    reportError(text);
}
void SportLibraryModel::applyPreference(const std::string& identifier, core::SportExercisePreference value)
{
    m_preferences[identifier] = value;
    m_error.clear();
    emit changed();
}
void SportLibraryModel::reportError(const QString& text)
{
    m_error = text;
    emit changed();
}
QVariantList SportLibraryModel::sections() const
{
    return {
        QVariantMap{{"key", "overview"}, {"title", "Обзор"}}, QVariantMap{{"key", "programs"}, {"title", "Программы"}},
        QVariantMap{{"key", "history"}, {"title", "История"}}, QVariantMap{{"key", "progress"}, {"title", "Прогресс"}}};
}
QString SportLibraryModel::section() const
{
    return m_section;
}
bool SportLibraryModel::showExercises() const
{
    return m_showExercises;
}
QString SportLibraryModel::summary() const
{
    const auto prepared = std::ranges::count_if(m_catalog, [](const auto& item) { return item.published; });

    return QString{"Офлайн-библиотека · подготовлено %1 из %2 карточек"}.arg(prepared).arg(m_catalog.size());
}
const core::SportExerciseRevision* SportLibraryModel::find(const std::string& identifier) const
{
    const auto iterator = std::ranges::find(m_catalog, identifier, &core::SportExerciseRevision::identifier);

    return iterator == m_catalog.end() ? nullptr : &*iterator;
}
core::SportExercisePreference SportLibraryModel::preference(const std::string& identifier) const
{
    const auto iterator = m_preferences.find(identifier);

    return iterator == m_preferences.end() ? core::SportExercisePreference{} : iterator->second;
}
bool SportLibraryModel::matches(const core::SportExerciseRevision& exercise) const
{
    if (!exercise.published)
    {
        return false;
    }
    const auto saved = preference(exercise.identifier);
    if ((m_favoritesOnly && !saved.favorite) || (m_progressionOnly && !saved.progression))
    {
        return false;
    }
    if ((m_noFloor && exercise.requirements.floorTransition.value_or(true)) ||
        (m_noPalmSupport && exercise.requirements.palmSupport.value_or(true)))
    {
        return false;
    }
    if (m_equipment == "Без инвентаря" && !exercise.equipment.empty())
    {
        return false;
    }
    if (m_equipment == "Есть гантели" || m_equipment == "Есть штанга")
    {
        const auto allowed = m_equipment == "Есть гантели" ? std::string{"Гантели"} : std::string{"Штанга"};
        if (!std::ranges::all_of(exercise.equipment, [allowed](const auto& item) { return item == allowed; }))
        {
            return false;
        }
    }
    if (!m_muscle.isEmpty() && m_muscle != "Все мышцы" &&
        std::ranges::none_of(exercise.muscles, [this](const auto& item) { return string(item) == m_muscle; }))
    {
        return false;
    }
    const auto searchable =
        normalized(string(exercise.title) + " " + joined(exercise.aliases) + " " + joined(exercise.muscles));
    for (const auto& word : normalized(m_query).split(' ', Qt::SkipEmptyParts))
    {
        if (!searchable.contains(word))
        {
            return false;
        }
    }

    return true;
}
QVariantList SportLibraryModel::exercises() const
{
    std::vector<const core::SportExerciseRevision*> filtered;
    for (const auto& exercise : m_catalog)
    {
        if (matches(exercise))
        {
            filtered.push_back(&exercise);
        }
    }
    std::ranges::sort(filtered,
                      [](const auto* first, const auto* second)
                      {
                          const auto titleOrder =
                              QString::localeAwareCompare(string(first->title), string(second->title));
                          return titleOrder == 0 ? first->identifier < second->identifier : titleOrder < 0;
                      });
    QVariantList result;
    for (const auto* exercise : filtered)
    {
        const auto saved = preference(exercise->identifier);
        QStringList badges;
        if (saved.favorite)
        {
            badges.append("Избранное");
        }
        if (saved.progression)
        {
            badges.append("Хочу прогрессировать");
        }
        result.append(QVariantMap{
            {"identifier", string(exercise->identifier)},
            {"title", string(exercise->title)},
            {"equipment", exercise->equipment.empty() ? QString{"Без инвентаря"} : joined(exercise->equipment)},
            {"badges", badges.join(" · ")}});
    }

    return result;
}
QString SportLibraryModel::query() const
{
    return m_query;
}
QStringList SportLibraryModel::equipmentOptions() const
{
    return {"Любой инвентарь", "Без инвентаря", "Есть гантели", "Есть штанга"};
}
QStringList SportLibraryModel::muscleOptions() const
{
    std::set<QString> muscles;
    for (const auto& item : m_catalog)
    {
        if (item.published)
        {
            for (const auto& muscle : item.muscles)
            {
                muscles.insert(string(muscle));
            }
        }
    }
    QStringList result{"Все мышцы"};
    for (const auto& muscle : muscles)
    {
        result.append(muscle);
    }

    return result;
}
int SportLibraryModel::equipmentIndex() const
{
    return static_cast<int>(std::max<qsizetype>(0, equipmentOptions().indexOf(m_equipment)));
}
int SportLibraryModel::muscleIndex() const
{
    return static_cast<int>(std::max<qsizetype>(0, muscleOptions().indexOf(m_muscle)));
}
bool SportLibraryModel::favoritesOnly() const
{
    return m_favoritesOnly;
}
bool SportLibraryModel::progressionOnly() const
{
    return m_progressionOnly;
}
bool SportLibraryModel::noFloor() const
{
    return m_noFloor;
}
bool SportLibraryModel::noPalmSupport() const
{
    return m_noPalmSupport;
}
bool SportLibraryModel::hasSelection() const
{
    const auto* exercise = m_snapshot ? &*m_snapshot : find(m_selected.toStdString());
    return exercise != nullptr && exercise->published;
}
QVariantMap SportLibraryModel::selected() const
{
    const auto* exercise = m_snapshot ? &*m_snapshot : find(m_selected.toStdString());
    if (exercise == nullptr || !exercise->published)
    {
        return {};
    }
    QStringList steps;
    int number{1};
    for (const auto& step : exercise->technique.steps)
    {
        steps.append(QString{"%1. %2"}.arg(number++).arg(string(step)));
    }
    QStringList conditions;
    conditions.append(exercise->requirements.palmSupport.value_or(true) ? "Опора на кисти"
                                                                        : "Без опоры весом на кисти");
    conditions.append(exercise->requirements.floorTransition.value_or(true) ? "Требуется выход на пол"
                                                                            : "Без выхода на пол");
    if (exercise->requirements.kneeSupport.value_or(false))
    {
        conditions.append("Опора на колени");
    }
    if (exercise->requirements.stableSupport.value_or(false))
    {
        conditions.append("Нужна устойчивая опора: " + string(exercise->requirements.supportType));
    }
    const auto loadText =
        exercise->loadKind == 1
            ? QString{"Записывайте массу одной гантели и количество гантелей."}
            : (exercise->loadKind == 2
                   ? QString{"Полный вес штанги вместе с грифом."}
                   : QString{"Собственный вес. Дополнение необязательно; отягощение и противовес взаимоисключающие."});
    QVariantList sources;
    for (const auto& source : exercise->sources)
    {
        sources.append(QVariantMap{
            {"title", string(source.title)}, {"url", string(source.url)}, {"date", string(source.checkedDate)}});
    }
    QVariantList alternatives;
    for (const auto& identifier : exercise->alternatives)
    {
        if (const auto* alternative = find(identifier))
        {
            alternatives.append(QVariantMap{{"identifier", string(identifier)},
                                            {"title", string(alternative->title)},
                                            {"ready", alternative->published}});
        }
    }
    const auto saved = preference(exercise->identifier);

    return {{"identifier", string(exercise->identifier)},
            {"title", string(exercise->title)},
            {"brief", string(exercise->technique.brief)},
            {"preparation", string(exercise->technique.preparation)},
            {"steps", steps.join("\n\n")},
            {"breathing", string(exercise->technique.breathing)},
            {"counting", string(exercise->technique.counting) + " " + loadText},
            {"muscles", joined(exercise->muscles)},
            {"equipment", exercise->equipment.empty() ? QString{"Без инвентаря"} : joined(exercise->equipment)},
            {"errors", joined(exercise->errors, "\n\n")},
            {"limitations", joined(exercise->limitations, "\n\n")},
            {"conditions", conditions.join(" · ")},
            {"sources", sources},
            {"alternatives", alternatives},
            {"favorite", saved.favorite},
            {"progression", saved.progression}};
}
QString SportLibraryModel::errorText() const
{
    return m_error;
}
bool SportLibraryModel::storageReady() const
{
    return m_storageReady;
}
void SportLibraryModel::setQuery(const QString& value)
{
    if (m_query != value)
    {
        m_query = value;
        emit changed();
    }
}
void SportLibraryModel::setFavoritesOnly(bool value)
{
    if (m_favoritesOnly != value)
    {
        m_favoritesOnly = value;
        emit changed();
    }
}
void SportLibraryModel::setProgressionOnly(bool value)
{
    if (m_progressionOnly != value)
    {
        m_progressionOnly = value;
        emit changed();
    }
}
void SportLibraryModel::setNoFloor(bool value)
{
    if (m_noFloor != value)
    {
        m_noFloor = value;
        emit changed();
    }
}
void SportLibraryModel::setNoPalmSupport(bool value)
{
    if (m_noPalmSupport != value)
    {
        m_noPalmSupport = value;
        emit changed();
    }
}
void SportLibraryModel::navigate(const QString& section)
{
    if (section != "overview" && section != "programs" && section != "history" && section != "progress")
    {
        return;
    }
    m_section = section;
    emit changed();
}
void SportLibraryModel::openExercises()
{
    m_showExercises = true;
    navigate("programs");
}
void SportLibraryModel::openPrograms()
{
    m_showExercises = false;
    navigate("programs");
}
void SportLibraryModel::selectExercise(const QString& identifier)
{
    m_snapshot.reset();
    const auto* exercise = find(identifier.toStdString());
    if (exercise == nullptr || !exercise->published)
    {
        reportError("Карточка этого варианта ещё готовится.");
        return;
    }
    m_selected = identifier;
    emit changed();
}
void SportLibraryModel::selectSnapshot(core::SportExerciseRevision exercise)
{
    m_selected = QString::fromStdString(exercise.identifier);
    m_snapshot = std::move(exercise);
    emit changed();
}
void SportLibraryModel::clearSelection()
{
    m_snapshot.reset();
    m_selected.clear();
    emit changed();
}
void SportLibraryModel::setEquipmentFilter(const QString& value)
{
    m_equipment = value;
    emit changed();
}
void SportLibraryModel::setMuscleFilter(const QString& value)
{
    m_muscle = value;
    emit changed();
}
void SportLibraryModel::requestFavorite()
{
    if (!m_storageReady || !hasSelection())
    {
        return;
    }
    const auto saved = preference(m_selected.toStdString());
    emit preferenceRequested(m_selected, !saved.favorite, saved.progression);
}
void SportLibraryModel::requestProgression()
{
    if (!m_storageReady || !hasSelection())
    {
        return;
    }
    const auto saved = preference(m_selected.toStdString());
    emit preferenceRequested(m_selected, saved.favorite, !saved.progression);
}
void SportLibraryModel::resetFilters()
{
    m_query.clear();
    m_equipment.clear();
    m_muscle.clear();
    m_favoritesOnly = false;
    m_progressionOnly = false;
    m_noFloor = false;
    m_noPalmSupport = false;
    emit changed();
}
void SportLibraryModel::retry()
{
    emit reloadRequested();
}
} // namespace msa::ui
