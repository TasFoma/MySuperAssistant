#include "msa/client/SportController.h"

#include "msa/infra/SportCatalog.h"

#include <exception>
#include <utility>

namespace msa::client
{
SportController::SportController(ui::SportLibraryModel& model, QString databasePath, QObject* parent) :
    QObject{parent}, m_model{model}, m_databasePath{std::move(databasePath)}
{
    connect(&m_model, &ui::SportLibraryModel::preferenceRequested, this, &SportController::onPreferenceRequested);
    connect(&m_model, &ui::SportLibraryModel::reloadRequested, this, &SportController::start);
}
void SportController::start()
{
    std::vector<core::SportExerciseRevision> catalog;
    try
    {
        catalog = infra::loadBundledSportCatalog();
        m_model.applyCatalog(catalog);
        if (m_databasePath.isEmpty())
        {
            m_model.applyData(catalog, {}, false);
            return;
        }
        m_store.reset();
        m_store = std::make_unique<infra::SqliteSportStore>(m_databasePath);
        m_store->importExercises(catalog);
        m_model.applyData(m_store->exercises(), m_store->preferences(), true);
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
} // namespace msa::client
