#pragma once

#include "msa/core/ClientPreferences.h"

#include <QString>
#include <optional>

namespace msa::infra
{
/// @brief Строгий локальный JSON клиента с атомарной заменой файла.
class JsonClientPreferences final
{
public:
    /// @brief Возвращает nullopt только при отсутствии файла; повреждения и ошибки чтения бросают исключение.
    [[nodiscard]] static std::optional<core::ClientPreferences> load(const QString& path);
    /// @brief Сохраняет проверенные параметры; родительский каталог должен существовать.
    static void save(const QString& path, const core::ClientPreferences& preferences);
};
} // namespace msa::infra
