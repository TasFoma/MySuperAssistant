#pragma once

#include "msa/core/ExternalServiceResult.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>

#include <chrono>

namespace msa::infra
{

/// @brief Общие параметры GET без личных адресов и значений по умолчанию.
struct HttpGetSettings
{
    /// @brief Положительный общий срок HTTP-запроса, включая получение тела.
    std::chrono::milliseconds timeout{};
    /// @brief Идентификация приложения для публичных поставщиков данных.
    QByteArray userAgent{};
};

/// @brief Вспомогательный синхронный GET для внешних сервисов.
/// Требует QCoreApplication и общий рабочий поток с переданным менеджером.
/// Локальный цикл событий допускает события потока: вызывающая связка исключает
/// повторный вход. Менеджер должен пережить клиент и все его вызовы.
class HttpGetClient final
{
public:
    /// @brief Проверяет параметры; неверные вызывают invalid_argument.
    HttpGetClient(QNetworkAccessManager& manager, HttpGetSettings settings);
    /// @brief Один GET без повторов/перенаправлений; interval ограничивает частоту.
    /// Срок HTTP не включает ожидание интервала. Ошибки не раскрывают URL и тело.
    [[nodiscard]] core::ExternalServiceResult<QByteArray>
    get(const QUrl& url, std::chrono::milliseconds interval = std::chrono::milliseconds{0});
    /// @brief Проверяет HTTP(S)-адрес без userInfo, query и fragment.
    [[nodiscard]] static bool validEndpoint(const QUrl& url);

private:
    QNetworkAccessManager& m_manager;
    HttpGetSettings m_settings{};
    std::chrono::steady_clock::time_point m_nextRequest{};
    bool m_inProgress{false};
};

} // namespace msa::infra
