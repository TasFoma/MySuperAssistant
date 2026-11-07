#pragma once

#include "msa/core/RoutePlanner.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace msa::core
{

/// @brief Сохранённый расчёт; не означает совершённую поездку.
struct RouteCalculation
{
    /// @brief Постоянный ID, ноль до сохранения.
    std::int64_t identifier{};
    /// @brief Местное время расчёта по часам сервера.
    std::chrono::local_seconds createdAt{};
    /// @brief Исходное название начала или «Моё местоположение».
    std::string originName{};
    /// @brief Исходное название назначения.
    std::string destinationName{};
    /// @brief Фактически использованные координаты начала.
    GeoCoordinates origin{};
    /// @brief Фактически использованные координаты назначения.
    GeoCoordinates destination{};
    /// @brief Способ передвижения.
    TravelMode mode{TravelMode::Foot};
    /// @brief Прогноз расстояния в метрах.
    double distanceMeters{};
    /// @brief Прогноз длительности в секундах.
    double durationSeconds{};
    /// @brief Локальная приблизительная оценка вместо ответа маршрутизатора.
    bool approximate{false};
};

/// @brief История расчётов без автоматического удаления и сортировки.
class RouteHistoryRepository
{
public:
    /// @brief Освобождает реализацию.
    virtual ~RouteHistoryRepository();
    /// @brief Сохраняет запись с нулевым ID и возвращает положительный ID.
    [[nodiscard]] virtual std::int64_t add(const RouteCalculation& calculation) = 0;
    /// @brief Независимые копии всех записей; технические ошибки — исключения.
    [[nodiscard]] virtual std::vector<RouteCalculation> all() const = 0;
};

} // namespace msa::core
