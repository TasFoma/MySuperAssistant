#pragma once

namespace msa::core {

/// @brief Географические координаты в градусах; нулевые значения допустимы.
struct GeoCoordinates {
    /// @brief Широта от -90 до 90 градусов.
    double latitude{0.0};
    /// @brief Долгота от -180 до 180 градусов.
    double longitude{0.0};
};

}  // namespace msa::core

