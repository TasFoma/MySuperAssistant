#include "msa/desktop/MusicController.h"

#include <QDir>
#include <QStandardPaths>

namespace msa::desktop
{
MusicController::MusicController(ui::MusicModel& model, infra::LocalMusicPlayer& player, QObject* parent) :
    QObject{parent}, m_player{player}
{
    connect(&model, &ui::MusicModel::commandRequested, &player, &infra::LocalMusicPlayer::execute);
    connect(&player, &infra::LocalMusicPlayer::stateChanged, &model, &ui::MusicModel::applyState);
    model.applyState(player.state());
}
QString MusicController::defaultSettingsPath()
{
    return QDir{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)}.filePath("music.settings.json");
}
void MusicController::restore()
{
    m_player.restore();
}
} // namespace msa::desktop
