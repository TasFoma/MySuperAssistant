#pragma once

#include "msa/core/MusicData.h"

#include <QString>

#include <optional>

namespace msa::infra
{
/// @brief Строгий отдельный JSON музыки с атомарным сохранением.
class JsonMusicPreferences final
{
public:
    /// @brief Отсутствие файла возвращает nullopt; ошибки не раскрывают его содержимое.
    [[nodiscard]] static std::optional<core::MusicPreferences> load(const QString& path);
    /// @brief Родительский каталог создаёт связка приложения или плеер при явном сохранении.
    static void save(const QString& path, const core::MusicPreferences& preferences);
};
} // namespace msa::infra
