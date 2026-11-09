#include "msa/core/MusicPlaylist.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace msa::core
{
MusicPlaylist::MusicPlaylist(std::uint32_t seed) : m_random{seed} {}
void MusicPlaylist::replace(MusicTracks tracks)
{
    if (std::ranges::any_of(tracks, [](const MusicTrack& track) { return track.path.empty(); }))
    {
        throw std::invalid_argument{"У трека должен быть локальный путь."};
    }
    std::shuffle(tracks.begin(), tracks.end(), m_random);
    m_tracks = std::move(tracks);
    m_index = 0;
    m_failures = 0;
}
const MusicTrack* MusicPlaylist::current() const
{
    return m_tracks.empty() ? nullptr : &m_tracks[m_index];
}
std::size_t MusicPlaylist::count() const
{
    return m_tracks.size();
}
std::size_t MusicPlaylist::position() const
{
    return m_tracks.empty() ? 0 : m_index + 1;
}
bool MusicPlaylist::next()
{
    if (m_tracks.empty())
    {
        return false;
    }
    m_failures = 0;
    m_index = (m_index + 1) % m_tracks.size();

    return true;
}
bool MusicPlaylist::previous()
{
    if (m_tracks.empty())
    {
        return false;
    }
    m_failures = 0;
    m_index = m_index == 0 ? m_tracks.size() - 1 : m_index - 1;

    return true;
}
bool MusicPlaylist::ended()
{
    return next();
}
bool MusicPlaylist::failed()
{
    if (m_tracks.empty() || m_failures >= m_tracks.size())
    {
        return false;
    }
    ++m_failures;
    if (m_failures == m_tracks.size())
    {
        return false;
    }
    m_index = (m_index + 1) % m_tracks.size();

    return true;
}
void MusicPlaylist::retry()
{
    m_failures = 0;
}
} // namespace msa::core
