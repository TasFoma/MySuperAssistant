#pragma once

#include "msa/core/ExternalServiceResult.h"
#include "msa/core/GeoCoordinates.h"

#include <optional>

namespace msa::core
{

/// @brief Прогноз на текущий местный день для запрошенной точки.
struct WeatherForecast
{
    /// @brief Температура сейчас, градусы Цельсия.
    double temperatureCelsius{};
    /// @brief Ощущаемая температура, градусы Цельсия.
    double apparentTemperatureCelsius{};
    /// @brief Код погоды WMO; неизвестные неотрицательные коды сохраняются.
    int weatherCode{};
    /// @brief Скорость ветра в км/ч.
    double windSpeedKilometersPerHour{};
    /// @brief Максимум дня; вместе с минимумом либо оба отсутствуют.
    std::optional<double> maximumTemperatureCelsius{};
    /// @brief Минимум дня.
    std::optional<double> minimumTemperatureCelsius{};
    /// @brief Вероятность осадков от 0 до 100 процентов, если доступна.
    std::optional<int> precipitationProbabilityPercent{};
};

/// @brief Погода по явным координатам без подстановки города по умолчанию.
class WeatherProvider
{
public:
    /// @brief Освобождает реализацию.
    virtual ~WeatherProvider();
    /// @brief Проверяет координаты и полноту ответа; не выполняет повторов.
    [[nodiscard]] ExternalServiceResult<WeatherForecast> forecast(GeoCoordinates coordinates);

protected:
    /// @brief Один запрос прогноза после проверки координат.
    [[nodiscard]] virtual ExternalServiceResult<WeatherForecast> requestForecast(GeoCoordinates coordinates) = 0;
};

} // namespace msa::core
