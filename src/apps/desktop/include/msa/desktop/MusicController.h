#pragma once

#include "msa/infra/LocalMusicPlayer.h"
#include "msa/ui/MusicModel.h"

#include <QObject>
#include <QString>

namespace msa::desktop
{
/// @brief Связывает локальную модель и аудио без зависимости от серверной сессии.
class MusicController final : public QObject
{
    Q_OBJECT
public:
    /// @brief Объекты модели и проигрывателя должны жить дольше контроллера.
    MusicController(ui::MusicModel& model, infra::LocalMusicPlayer& player, QObject* parent = nullptr);
    /// @brief Отдельный файл в пользовательском каталоге приложения.
    [[nodiscard]] static QString defaultSettingsPath();
    /// @brief Явное восстановление после создания связки, без автозапуска трека.
    void restore();

private:
    infra::LocalMusicPlayer& m_player;
};
} // namespace msa::desktop
