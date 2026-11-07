#pragma once

#include "msa/core/Geocoder.h"

#include <chrono>
#include <optional>

namespace msa::core
{

/// @brief Снимок кандидатов; пустой список — сохранённое отсутствие места.
struct GeocodingCacheEntry
{
    /// @brief Местное время получения ответа по часам сервера.
    std::chrono::local_seconds storedAt{};
    /// @brief Все кандидаты, без закрепления выбора за местоположением пользователя.
    std::vector<GeocodedPlace> places{};
};

/// @brief Хранилище кэша по поставщику/параметрам поиска и полному запросу.
class GeocodingCacheRepository
{
public:
    /// @brief Освобождает реализацию.
    virtual ~GeocodingCacheRepository();
    /// @brief Отсутствующий ключ — nullopt; технические ошибки — исключения.
    [[nodiscard]] virtual std::optional<GeocodingCacheEntry> find(const std::string& source,
                                                                  const std::string& query) const = 0;
    /// @brief Атомарно заменяет снимок, включая пустой список; старые ключи не удаляет.
    virtual void save(const std::string& source, const std::string& query, const GeocodingCacheEntry& entry) = 0;
};

} // namespace msa::core
