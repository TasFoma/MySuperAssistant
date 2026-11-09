#pragma once

#include "msa/core/ClientData.h"

#include <QJsonObject>
#include <QString>

namespace msa::infra
{
/// @brief Готовые метод и параметры; ID назначает TcpClient непосредственно при отправке.
struct ClientWireRequest
{
    QString method;
    QJsonObject params;
};

/// @brief Граница JSON и типизированных клиентских данных, без обращения к сети или UI.
class ClientProtocol
{
public:
    /// @brief Проверяет и кодирует чтение; неверные поля вызывают invalid_argument.
    [[nodiscard]] static ClientWireRequest request(const core::ClientReadRequest& request);
    /// @brief Кодирует явное пользовательское действие без повторов.
    [[nodiscard]] static ClientWireRequest request(const core::ClientAction& action);
    /// @brief Полностью разбирает результат ожидаемого чтения, включая вложенные объекты.
    /// Неверные типы, поля, дубли ID, адрес/курсор страницы вызывают invalid_argument без тела JSON.
    [[nodiscard]] static core::ClientReadResult decode(const core::ClientReadRequest& request,
                                                       const QJsonObject& result);
    /// @brief Проверяет подтверждение действия до обновления представления; неверное вызывает invalid_argument.
    static void validateActionResult(const core::ClientAction& action, const QJsonObject& result);
};
} // namespace msa::infra
