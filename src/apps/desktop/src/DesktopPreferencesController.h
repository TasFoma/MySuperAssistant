#pragma once

#include "msa/ui/DesktopState.h"

#include <QObject>
#include <QString>

namespace msa::desktop
{
/// @brief Связывает локальный файл в infra с формой UI, не включая сеть.
class DesktopPreferencesController final : public QObject
{
public:
    /// @brief Путь передаёт точка сборки; в тестах используется временный каталог.
    DesktopPreferencesController(ui::DesktopState& state, QString path, QObject* parent = nullptr);
    /// @brief Загружает настройки; при отсутствии/повреждении открывает их форму.
    void load();
    /// @brief Путь client.settings.json в QStandardPaths::AppConfigLocation.
    [[nodiscard]] static QString defaultPath();

private:
    ui::DesktopState& m_state;
    QString m_path;
};
} // namespace msa::desktop
