#pragma once

#include "msa/core/MusicData.h"

#include <cstddef>
#include <cstdint>
#include <random>

namespace msa::core
{
/// @brief Перемешанная очередь с циклическими переходами и конечным проходом ошибок.
class MusicPlaylist final
{
public:
    /// @brief Генератор принадлежит очереди; явный seed позволяет воспроизводимые тесты.
    explicit MusicPlaylist(std::uint32_t seed);
    /// @brief Проверяет все треки, заменяет очередь и перемешивает её с начала.
    void replace(MusicTracks tracks);
    /// @brief Текущий трек; указатель невладеющий и действует до изменения очереди.
    [[nodiscard]] const MusicTrack* current() const;
    /// @brief Число треков и позиция от единицы; пустая очередь имеет позицию 0.
    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] std::size_t position() const;
    /// @brief Ручные переходы начинают новый проход попыток.
    [[nodiscard]] bool next();
    [[nodiscard]] bool previous();
    /// @brief Нормальное завершение начинает новый проход с последующего трека.
    [[nodiscard]] bool ended();
    /// @brief Ошибка переходит дальше; false после одного полного неуспешного прохода.
    [[nodiscard]] bool failed();
    /// @brief Явное повторное воспроизведение сбрасывает счёт ошибок без смены трека.
    void retry();

private:
    MusicTracks m_tracks{};
    std::mt19937 m_random;
    std::size_t m_index{};
    std::size_t m_failures{};
};
} // namespace msa::core
