#pragma once

#include "msa/core/AudioDeviceData.h"

#include <QString>

#include <optional>

namespace msa::infra
{
/// @brief Атомарные локальные настройки устройств; идентификаторы закодированы Base64.
class JsonAudioDevicePreferences final
{
public:
    /// @brief Отсутствие файла возвращает nullopt; повреждение отклоняется исключением.
    [[nodiscard]] static std::optional<core::AudioDevicePreferences> load(const QString& path);
    /// @brief Записывает version=1, input_device и output_device; пустой ID означает системный выбор.
    static void save(const QString& path, const core::AudioDevicePreferences& preferences);
    /// @brief Проверяет ограниченный по длине канонический Base64 либо пустой ID.
    [[nodiscard]] static bool validIdentifier(const QString& identifier);
};
} // namespace msa::infra
