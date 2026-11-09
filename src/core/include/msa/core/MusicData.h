#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace msa::core
{
/// @brief Локальный трек; путь и название передаёт инфраструктура сканирования.
struct MusicTrack
{
    std::string path{};
    std::string title{};
    bool operator==(const MusicTrack&) const = default;
};
using MusicTracks = std::vector<MusicTrack>;
/// @brief Независимые от сервера постоянные параметры музыки.
struct MusicPreferences
{
    std::string folder{};
    double volume{0.7};
};
/// @brief Снимок музыкального представления без типов Qt.
struct MusicState
{
    std::string folder{};
    std::string title{};
    std::size_t position{};
    std::size_t count{};
    double volume{0.7};
    bool playing{false};
    bool scanning{false};
    std::string error{};
};
/// @brief Явные локальные действия, не требующие клиентской TCP-сессии.
enum class MusicCommandKind
{
    ChooseFolder,
    PlayPause,
    Next,
    Previous,
    Reshuffle,
    SetVolume
};
struct MusicCommand
{
    MusicCommandKind kind{MusicCommandKind::PlayPause};
    std::string folder{};
    double volume{0.7};
};
} // namespace msa::core
