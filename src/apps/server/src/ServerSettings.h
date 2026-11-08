#pragma once

#include "msa/infra/HttpGetClient.h"
#include "msa/infra/LlamaClient.h"
#include "msa/infra/OsrmRoutePlanner.h"

#include <QHostAddress>
#include <QString>
#include <QUrl>

#include <cstddef>

namespace msa::server {

/// @brief Явные настройки запуска; секреты и личные адреса не имеют значений по умолчанию.
struct ServerSettings {
    /// @brief Числовой адрес интерфейса, без DNS и неявного выбора сети.
    QHostAddress listenAddress{};
    /// @brief Порт 0–65535; ноль поручает выбор ОС.
    quint16 port{};
    /// @brief Общий токен авторизации, никогда не выводится в журнал.
    QString token{};
    /// @brief IANA-зона местного времени core и протокола.
    QString timeZone{};
    /// @brief Разрешённый абсолютный путь к SQLite, каталог должен существовать.
    QString databasePath{};
    /// @brief Общий бюджет кодовых точек, не токенов модели.
    std::size_t contextMaxChars{};
    /// @brief Период проверок сроков в миллисекундах.
    int checkIntervalMs{};
    /// @brief Параметры одного полного запроса llama-server.
    infra::LlamaClientSettings llama{};
    /// @brief Общий срок GET и идентификация приложения.
    infra::HttpGetSettings http{};
    /// @brief Полный endpoint поиска кандидатов места.
    QUrl nominatim{};
    /// @brief Префиксы маршрутов для трёх видов передвижения.
    infra::OsrmEndpoints osrm{};
    /// @brief Полный endpoint прогноза Open-Meteo.
    QUrl weather{};
    /// @brief Полный endpoint JSON-поиска SearXNG.
    QUrl search{};
};

/// @brief Читает строгий JSON; относительный путь БД разрешается от каталога настроек.
/// Неверный файл вызывает invalid_argument без содержимого файла и личных значений.
[[nodiscard]] ServerSettings readServerSettings(const QString& filePath);

} // namespace msa::server

