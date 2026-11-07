#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>
#include <variant>

namespace msa::infra
{
/// Версия и пределы согласованного протокола; размер не включает LF.
inline constexpr int kProtocolVersion{1};
inline constexpr int kProtocolMaxFrameBytes{1048576};
inline constexpr int kProtocolAuthTimeoutSeconds{10};

/// Запрос авторизации; проверка токена и поддерживаемой версии принадлежит серверу.
struct ProtocolAuth
{
    QString requestId;
    int version{};
    QString token;
};

/// Запрос операции. params проверяются по каталогу версии 1 до исполнения.
struct ProtocolRequest
{
    QString requestId;
    QString method;
    QJsonObject params;
};

/// Успех операции; предметный result формирует обработчик указанного запроса.
struct ProtocolSuccess
{
    QString requestId;
    QJsonObject result;
};

/// Отказ операции с явно указанными последствиями записи.
struct ProtocolFailure
{
    QString requestId;
    QString code;
    QString message;
    QString effect;
    std::optional<QString> userMessageId;
};

/// Уведомление об уже выполненном изменении.
struct ProtocolEvent
{
    QString serverInstanceId;
    QString eventId;
    QString name;
    QJsonObject data;
};

/// Ошибка соединения, после которой транспорт обязан закрыть сессию.
struct ProtocolError
{
    QString code;
    QString message;
};

/// Взаимоисключающие конверты сообщений версии 1.
using ProtocolMessage =
    std::variant<ProtocolAuth, ProtocolRequest, ProtocolSuccess, ProtocolFailure, ProtocolEvent, ProtocolError>;

/// Ошибка разбора. requestId задан только при надёжно прочитанном идентификаторе.
struct ProtocolDecodeError
{
    QString code;
    std::optional<QString> requestId;
    bool closeConnection{};
};

/// Либо целое проверенное сообщение, либо причина отказа без частичного исполнения.
using ProtocolDecodeResult = std::variant<ProtocolMessage, ProtocolDecodeError>;
} // namespace msa::infra
