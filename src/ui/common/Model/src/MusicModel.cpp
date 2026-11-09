#include "msa/ui/MusicModel.h"

#include <cmath>

namespace msa::ui
{
namespace
{
constexpr double kPercentScale = 100;
}
MusicModel::MusicModel(QObject* parent) : QObject{parent} {}
void MusicModel::applyState(const core::MusicState& state)
{
    m_state = state;
    emit changed();
}
QString MusicModel::folder() const
{
    return QString::fromStdString(m_state.folder);
}
QString MusicModel::title() const
{
    if (m_state.scanning)
    {
        return "Ищем музыку…";
    }

    return m_state.count == 0 ? "Выберите папку с музыкой" : QString::fromStdString(m_state.title);
}
QString MusicModel::positionText() const
{
    return m_state.count == 0 ? "Нет треков" : QString{"%1 / %2"}.arg(m_state.position).arg(m_state.count);
}
QString MusicModel::errorText() const
{
    return QString::fromStdString(m_state.error);
}
bool MusicModel::playing() const
{
    return m_state.playing;
}
bool MusicModel::scanning() const
{
    return m_state.scanning;
}
bool MusicModel::canControl() const
{
    return m_state.count > 0 && !m_state.scanning;
}
double MusicModel::volumePercent() const
{
    return m_state.volume * kPercentScale;
}
QString MusicModel::volumeText() const
{
    return QString{"%1%"}.arg(std::lround(volumePercent()));
}
void MusicModel::setVolumePercent(double percent)
{
    if (!std::isfinite(percent) || percent < 0 || percent > kPercentScale)
    {
        return;
    }
    emit commandRequested({core::MusicCommandKind::SetVolume, {}, percent / kPercentScale});
}
void MusicModel::chooseFolder(const QString& folder)
{
    emit commandRequested({core::MusicCommandKind::ChooseFolder, folder.toStdString()});
}
void MusicModel::playPause()
{
    if (canControl())
    {
        emit commandRequested({core::MusicCommandKind::PlayPause});
    }
}
void MusicModel::next()
{
    if (canControl())
    {
        emit commandRequested({core::MusicCommandKind::Next});
    }
}
void MusicModel::previous()
{
    if (canControl())
    {
        emit commandRequested({core::MusicCommandKind::Previous});
    }
}
void MusicModel::reshuffle()
{
    if (!m_state.folder.empty() && !scanning())
    {
        emit commandRequested({core::MusicCommandKind::Reshuffle});
    }
}
} // namespace msa::ui
