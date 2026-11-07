#pragma once

#include "msa/core/ExternalServiceResult.h"
#include "msa/core/GeoCoordinates.h"

#include <string>
#include <vector>

namespace msa::core
{

/// @brief Найденное место с полным названием поставщика данных.
struct GeocodedPlace
{
    /// @brief Корректные координаты; нули допустимы.
    GeoCoordinates coordinates{};
    /// @brief Полное название без сокращения адреса.
    std::string name{};
};

/// @brief Поиск кандидатов без выбора местоположения пользователя.
class Geocoder
{
public:
    /// @brief Освобождает реализацию.
    virtual ~Geocoder();
    /// @brief Ищет место по непустому названию; пустой список означает NotFound.
    [[nodiscard]] ExternalServiceResult<std::vector<GeocodedPlace>> search(const std::string& place);

protected:
    /// @brief Одна попытка поиска после проверки и trim названия.
    [[nodiscard]] virtual ExternalServiceResult<std::vector<GeocodedPlace>> requestPlaces(const std::string& place) = 0;
};

} // namespace msa::core
