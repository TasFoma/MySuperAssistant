#pragma once

#include "msa/infra/SqliteSportStore.h"
#include "msa/ui/SportLibraryModel.h"

#include <QObject>
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

private:
    void onPreferenceRequested(const QString& identifier, bool favorite, bool progression);

private:
    ui::SportLibraryModel& m_model;
    QString m_databasePath{};
    std::unique_ptr<infra::SqliteSportStore> m_store{};
};
} // namespace msa::client
