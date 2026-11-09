#pragma once

#include <QString>

#include <optional>

namespace msa::infra
{
/// @brief Отдельный атомарный JSON выбранной папки модели, без запуска распознавания.
class JsonVoicePreferences final
{
public:
    /// @brief Отсутствие возвращает nullopt; схема/чтение отклоняются исключением.
    [[nodiscard]] static std::optional<QString> load(const QString& path);
    /// @brief version=1 и model_path; пустой путь выключает конфигурацию микрофона.
    static void save(const QString& path, const QString& modelPath);
};
} // namespace msa::infra
