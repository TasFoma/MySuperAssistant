#include "msa/infra/TcpServer.h"

#include "msa/infra/ProtocolPayload.h"

#include <QTimeZone>
#include <QUuid>

#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace msa::infra
{
namespace
{
constexpr qint64 kReadChunkBytes{65536};
constexpr int kMillisecondsPerSecond{1000};

[[nodiscard]] ProtocolFailure rejection(const QString& requestId, const QString& code)
{
    return {requestId, code, QStringLiteral("Запрос отклонён сервером."), "none", std::nullopt};
}
} // namespace

TcpServer::TcpServer(QString token, QString timeZone, const core::Clock& clock, QObject* parent) :
    QObject{parent}, m_token{std::move(token)}, m_timeZone{std::move(timeZone)}, m_clock{clock},
    m_serverInstanceId{QUuid::createUuid().toString(QUuid::WithoutBraces)}
{
    if (m_token.trimmed().isEmpty() || !QTimeZone::isTimeZoneIdAvailable(m_timeZone.toUtf8()))
    {
        throw std::invalid_argument("Invalid TCP server settings");
    }
    qRegisterMetaType<ProtocolRequest>();
    connect(&m_listener, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    // Не вызываем внешние обработчики connectionClosed из деструктора.
    for (auto& [connectionId, session] : m_sessions)
    {
        static_cast<void>(connectionId);
        session.socket->disconnect(this);
        session.authTimer->disconnect(this);
        session.socket->abort();
    }
}

bool TcpServer::listen(const QHostAddress& address, quint16 port)
{
    return m_listener.listen(address, port);
}

void TcpServer::close()
{
    m_listener.close();
    while (!m_sessions.empty())
    {
        const auto connectionId = m_sessions.begin()->first;
        auto* socket = m_sessions.begin()->second.socket;
        socket->abort();
        forgetConnection(connectionId);
    }
}

quint16 TcpServer::serverPort() const
{
    return m_listener.serverPort();
}

QString TcpServer::serverInstanceId() const
{
    return m_serverInstanceId;
}

void TcpServer::onNewConnection()
{
    while (m_listener.hasPendingConnections())
    {
        auto* socket = m_listener.nextPendingConnection();
        if (socket->state() == QAbstractSocket::UnconnectedState)
        {
            socket->deleteLater();
            continue;
        }
        if (m_nextConnectionId == std::numeric_limits<quint64>::max())
        {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        const auto connectionId = m_nextConnectionId++;
        socket->setParent(this);
        socket->setReadBufferSize(kProtocolMaxFrameBytes + 1);
        auto* timer = new QTimer(socket);
        timer->setSingleShot(true);
        timer->setTimerType(Qt::PreciseTimer);
        Session session;
        session.socket = socket;
        session.authTimer = timer;
        m_sessions.emplace(connectionId, std::move(session));
        connect(socket, &QTcpSocket::readyRead, this, [this, connectionId]() { readConnection(connectionId); });
        connect(socket, &QTcpSocket::disconnected, this, [this, connectionId]() { forgetConnection(connectionId); });
        connect(timer, &QTimer::timeout, this, [this, connectionId]() { failProtocol(connectionId); });
        timer->start(kProtocolAuthTimeoutSeconds * kMillisecondsPerSecond);
        readConnection(connectionId);
    }
}

void TcpServer::readConnection(quint64 connectionId)
{
    try
    {
        while (true)
        {
            const auto found = m_sessions.find(connectionId);
            if (found == m_sessions.end() || found->second.closing || found->second.socket->bytesAvailable() == 0)
            {
                return;
            }
            const auto bytes = found->second.socket->read(kReadChunkBytes);
            const auto batch = found->second.decoder.append(bytes);
            for (const auto& frame : batch.frames)
            {
                const auto current = m_sessions.find(connectionId);
                if (current == m_sessions.end() || current->second.closing)
                {
                    return;
                }
                processFrame(connectionId, frame);
            }
            if (batch.invalidFrame)
            {
                failProtocol(connectionId);
                return;
            }
        }
    }
    catch (...)
    {
        // Не выпускаем исключения из сетевого обработчика в цикл событий Qt.
        closeConnection(connectionId);
    }
}

bool TcpServer::rememberRequest(quint64 connectionId, const QString& requestId)
{
    auto& session = m_sessions.at(connectionId);
    if (!session.usedRequestIds.insert(requestId).second)
    {
        static_cast<void>(writeMessage(connectionId, rejection(requestId, "invalid_request")));
        return false;
    }

    return true;
}

void TcpServer::processFrame(quint64 connectionId, const QByteArray& frame)
{
    const auto decoded = ProtocolCodec::decode(frame);
    auto& session = m_sessions.at(connectionId);
    if (const auto* error = std::get_if<ProtocolDecodeError>(&decoded))
    {
        if (!session.authenticated || error->closeConnection)
        {
            failProtocol(connectionId);
            return;
        }
        if (rememberRequest(connectionId, *error->requestId))
        {
            static_cast<void>(writeMessage(connectionId, rejection(*error->requestId, error->code)));
        }
        return;
    }
    const auto& message = std::get<ProtocolMessage>(decoded);
    if (const auto* auth = std::get_if<ProtocolAuth>(&message))
    {
        if (!rememberRequest(connectionId, auth->requestId))
        {
            return;
        }
        if (session.authenticated)
        {
            static_cast<void>(writeMessage(connectionId, rejection(auth->requestId, "invalid_request")));
            return;
        }
        if (auth->version != kProtocolVersion || auth->token != m_token)
        {
            const auto code = auth->version != kProtocolVersion ? "unsupported_version" : "unauthorized";
            static_cast<void>(writeMessage(connectionId, rejection(auth->requestId, code)));
            closeConnection(connectionId);
            return;
        }
        const QJsonObject result{{"version", kProtocolVersion},
                                 {"server_instance_id", m_serverInstanceId},
                                 {"max_frame_bytes", kProtocolMaxFrameBytes},
                                 {"server_time", ProtocolPayload::localDateTime(m_clock.now())},
                                 {"time_zone", m_timeZone}};
        if (writeMessage(connectionId, ProtocolSuccess{auth->requestId, result}))
        {
            session.authenticated = true;
            session.authTimer->stop();
        }
        return;
    }
    if (!session.authenticated || !std::holds_alternative<ProtocolRequest>(message))
    {
        failProtocol(connectionId);
        return;
    }
    const auto request = std::get<ProtocolRequest>(message);
    if (!rememberRequest(connectionId, request.requestId))
    {
        return;
    }
    session.pendingRequestIds.insert(request.requestId);
    try
    {
        emit requestReceived(connectionId, request);
    }
    catch (...)
    {
        // Обработчик мог успеть записать данные до исключения: отсутствие эффекта неизвестно.
        static_cast<void>(
            sendResponse(connectionId, ProtocolFailure{request.requestId, "storage_error",
                                                       QStringLiteral("Не удалось подтвердить результат операции."),
                                                       "unknown", std::nullopt}));
    }
}

bool TcpServer::writeMessage(quint64 connectionId, const ProtocolMessage& message)
{
    const auto found = m_sessions.find(connectionId);
    if (found == m_sessions.end() || found->second.closing)
    {
        return false;
    }
    const auto encoded = ProtocolCodec::encode(message);
    const auto* bytes = std::get_if<QByteArray>(&encoded);
    if (!bytes)
    {
        return false;
    }
    if (found->second.socket->write(*bytes) != bytes->size())
    {
        closeConnection(connectionId);
        return false;
    }

    return true;
}

bool TcpServer::completeRequest(quint64 connectionId, const QString& requestId, const ProtocolMessage& response)
{
    const auto found = m_sessions.find(connectionId);
    if (found == m_sessions.end() || found->second.closing || !found->second.authenticated ||
        !found->second.pendingRequestIds.contains(requestId))
    {
        return false;
    }
    const auto encoded = ProtocolCodec::encode(response);
    if (const auto* error = std::get_if<ProtocolDecodeError>(&encoded))
    {
        if (error->code != "response_too_large")
        {
            return false;
        }
        auto fallback = ProtocolFailure{
            requestId, "response_too_large",
            QStringLiteral("Ответ превышает предел сообщения. Перечитайте данные отдельными запросами."), "unknown",
            std::nullopt};
        if (const auto* failure = std::get_if<ProtocolFailure>(&response))
        {
            fallback.effect = failure->effect;
            fallback.userMessageId = failure->userMessageId;
        }
        if (!writeMessage(connectionId, fallback))
        {
            return false;
        }
    }
    else if (!writeMessage(connectionId, response))
    {
        return false;
    }
    m_sessions.at(connectionId).pendingRequestIds.erase(requestId);

    return true;
}

bool TcpServer::sendResponse(quint64 connectionId, const ProtocolSuccess& response)
{
    return completeRequest(connectionId, response.requestId, response);
}

bool TcpServer::sendResponse(quint64 connectionId, const ProtocolFailure& response)
{
    return completeRequest(connectionId, response.requestId, response);
}

bool TcpServer::publishEvent(const QString& name, const QJsonObject& data)
{
    const ProtocolEvent event{m_serverInstanceId, QUuid::createUuid().toString(QUuid::WithoutBraces), name, data};
    if (!std::holds_alternative<QByteArray>(ProtocolCodec::encode(event)))
    {
        return false;
    }
    // write может синхронно закрыть сокет; список адресатов не зависит от удаления сессии.
    std::vector<quint64> recipients;
    for (const auto& [connectionId, session] : m_sessions)
    {
        if (session.authenticated && !session.closing)
        {
            recipients.push_back(connectionId);
        }
    }
    for (const auto connectionId : recipients)
    {
        static_cast<void>(writeMessage(connectionId, event));
    }

    return true;
}

void TcpServer::failProtocol(quint64 connectionId)
{
    static_cast<void>(writeMessage(
        connectionId,
        ProtocolError{"invalid_frame", QStringLiteral("Неверный формат сообщения или порядок авторизации.")}));
    closeConnection(connectionId);
}

void TcpServer::closeConnection(quint64 connectionId)
{
    const auto found = m_sessions.find(connectionId);
    if (found == m_sessions.end() || found->second.closing)
    {
        return;
    }
    found->second.closing = true;
    found->second.authTimer->stop();
    found->second.socket->disconnectFromHost();
    const auto current = m_sessions.find(connectionId);
    if (current != m_sessions.end() && current->second.socket->state() == QAbstractSocket::UnconnectedState)
    {
        forgetConnection(connectionId);
    }
}

void TcpServer::forgetConnection(quint64 connectionId)
{
    const auto found = m_sessions.find(connectionId);
    if (found == m_sessions.end())
    {
        return;
    }
    found->second.socket->deleteLater();
    m_sessions.erase(found);
    try
    {
        emit connectionClosed(connectionId);
    }
    catch (...)
    {
        // Сессия уже освобождена; исключение потребителя не должно останавливать цикл Qt.
    }
}
} // namespace msa::infra
