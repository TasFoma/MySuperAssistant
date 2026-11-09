#include "msa/infra/TcpClient.h"

#include <QDate>
#include <QJsonArray>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

namespace msa::infra {
namespace {
constexpr qint64 kReadChunkBytes{65536};
constexpr int kLocalTimeLength{19};

bool fields(const QJsonObject& object, std::initializer_list<const char*> names)
{
    if (object.size() != static_cast<qsizetype>(names.size())) { return false; }

    return std::ranges::all_of(names, [object](const char* name) { return object.contains(QLatin1String{name}); });
}

bool validLocalTime(const QJsonValue& value)
{
    if (!value.isString()) { return false; }
    const auto text = value.toString();
    if (text.size() != kLocalTimeLength || text.at(10) != 'T') { return false; }
    const auto date = QDate::fromString(text.first(10), Qt::ISODate);
    const auto time = QTime::fromString(text.sliced(11), "HH:mm:ss");

    return date.isValid() && time.isValid()
        && date.toString(Qt::ISODate) + 'T' + time.toString("HH:mm:ss") == text;
}

std::optional<TcpClientServerInfo> authInfo(const QJsonObject& result)
{
    if (!fields(result, {"version", "server_instance_id", "max_frame_bytes", "server_time", "time_zone"})
        || result.value("version").toInteger(-1) != kProtocolVersion
        || result.value("version").toDouble(-1) != kProtocolVersion
        || result.value("max_frame_bytes").toInteger(-1) != kProtocolMaxFrameBytes
        || result.value("max_frame_bytes").toDouble(-1) != kProtocolMaxFrameBytes
        || !result.value("server_instance_id").isString()
        || result.value("server_instance_id").toString().trimmed().isEmpty()
        || !result.value("time_zone").isString()
        || !QTimeZone::isTimeZoneIdAvailable(result.value("time_zone").toString().toUtf8())
        || !validLocalTime(result.value("server_time"))) { return std::nullopt; }

    return TcpClientServerInfo{result.value("server_instance_id").toString(),
        result.value("server_time").toString(), result.value("time_zone").toString(), kProtocolMaxFrameBytes};
}

bool validSnapshot(const QJsonObject& result, const TcpClientServerInfo& info)
{
    if (!fields(result, {"server_time", "time_zone", "main_chat_id", "tasks", "reminders", "habits", "events_today"})
        || !validLocalTime(result.value("server_time")) || result.value("time_zone") != info.timeZone
        || !ProtocolCodec::isEntityId(result.value("main_chat_id"))) { return false; }
    for (const auto* name : {"tasks", "reminders", "habits", "events_today"}) {
        const auto value = result.value(QLatin1String{name});
        if (!value.isArray() || !std::ranges::all_of(value.toArray(), [](const QJsonValue& item) { return item.isObject(); })) {
            return false;
        }
    }

    return true;
}

bool readOnly(const QString& method)
{
    return method == "ping" || method == "get_all" || method == "chat.list" || method == "chat.messages"
        || method == "profile.get" || method == "mood.history" || method == "route.history"
        || method == "calendar.day" || method == "calendar.month" || method == "weather.get";
}

bool validDuration(std::chrono::milliseconds value)
{
    return value.count() > 0 && value.count() <= std::numeric_limits<int>::max();
}
} // namespace

TcpClient::TcpClient(TcpClientSettings settings, QObject* parent)
    : QObject{parent}, m_settings{std::move(settings)}
{
    if (m_settings.host.trimmed().isEmpty() || m_settings.host.contains(QChar::Null)
        || m_settings.port == 0 || m_settings.token.trimmed().isEmpty()
        || !validDuration(m_settings.connectTimeout) || !validDuration(m_settings.responseTimeout)
        || !validDuration(m_settings.initialReconnectDelay) || !validDuration(m_settings.maximumReconnectDelay)
        || m_settings.initialReconnectDelay > m_settings.maximumReconnectDelay) {
        throw std::invalid_argument{"Неверные настройки TCP-клиента"};
    }
    qRegisterMetaType<TcpClientState>();
    qRegisterMetaType<TcpClientServerInfo>();
    qRegisterMetaType<TcpClientRequestFailure>();
    qRegisterMetaType<ProtocolSuccess>();
    qRegisterMetaType<ProtocolFailure>();
    qRegisterMetaType<ProtocolEvent>();
    for (auto* timer : {&m_retryTimer, &m_phaseTimer, &m_requestTimer}) {
        timer->setSingleShot(true);
        timer->setTimerType(Qt::PreciseTimer);
    }
    connect(&m_retryTimer, &QTimer::timeout, this, &TcpClient::onRetry);
    connect(&m_phaseTimer, &QTimer::timeout, this, &TcpClient::onPhaseTimeout);
    connect(&m_requestTimer, &QTimer::timeout, this, &TcpClient::onRequestTimeout);
}

TcpClient::~TcpClient()
{
    ++m_generation;
    m_enabled = false;
    m_retryTimer.stop();
    m_phaseTimer.stop();
    m_requestTimer.stop();
    detachSocket();
}

void TcpClient::connectToServer()
{
    if (m_state != TcpClientState::Disconnected && m_state != TcpClientState::Exhausted) { return; }
    m_attempts = 0;
    m_enabled = true;
    beginAttempt();
}

void TcpClient::reconnect()
{
    disconnectFromServer();
    connectToServer();
}

void TcpClient::disconnectFromServer()
{
    m_enabled = false;
    endSession("connection_closed", "Соединение отключено. Перечитайте данные перед повтором записи.", false, false);
}

void TcpClient::beginAttempt()
{
    if (!m_enabled) { return; }
    const auto generation = ++m_generation;
    m_retryTimer.stop();
    detachSocket();
    m_decoder.reset();
    ++m_attempts;
    m_authRequest = QString{"auth-%1"}.arg(generation);
    m_syncRequest = QString{"snapshot-%1"}.arg(generation);
    m_socket = new QTcpSocket(this);
    m_socket->setReadBufferSize(kProtocolMaxFrameBytes + 1);
    connect(m_socket, &QTcpSocket::connected, this, [this, generation]() {
        if (m_generation == generation) { onConnected(generation); }
    });
    connect(m_socket, &QTcpSocket::readyRead, this, [this, generation]() {
        if (m_generation == generation) { readSocket(generation); }
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this, generation]() {
        if (m_generation == generation) { readSocket(generation); }
        if (m_generation == generation) {
            endSession("connection_lost", "Соединение потеряно. Результат отправленной записи неизвестен.", true, true);
        }
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this, generation](QAbstractSocket::SocketError) {
        if (m_generation == generation) { readSocket(generation); }
        if (m_generation == generation) {
            endSession("connection_error", "Не удалось связаться с сервером.", true, true);
        }
    });
    m_phaseTimer.start(static_cast<int>(m_settings.connectTimeout.count()));
    setState(TcpClientState::Connecting);
    if (m_generation != generation) { return; }
    emit connectionAttempt(m_attempts);
    if (m_generation != generation) { return; }
    m_socket->connectToHost(m_settings.host, m_settings.port);
}

void TcpClient::onConnected(quint64 generation)
{
    m_phaseTimer.start(static_cast<int>(m_settings.connectTimeout.count()));
    setState(TcpClientState::Authenticating);
    if (m_generation != generation) { return; }
    const auto encoded = ProtocolCodec::encode(ProtocolAuth{m_authRequest, kProtocolVersion, m_settings.token});
    if (const auto* bytes = std::get_if<QByteArray>(&encoded)) {
        if (!writeBytes(*bytes) && m_generation == generation) {
            endSession("connection_error", "Не удалось отправить авторизацию.", true, true);
        }
    } else { endSession("invalid_settings", "Не удалось подготовить авторизацию.", false, true); }
}

QString TcpClient::nextRequestId()
{
    if (m_nextRequest == std::numeric_limits<quint64>::max()) {
        throw std::overflow_error{"Исчерпаны идентификаторы TCP-клиента"};
    }

    return QString{"request-%1"}.arg(m_nextRequest++);
}

QString TcpClient::sendRequest(const QString& method, const QJsonObject& params)
{
    const auto requestId = nextRequestId();
    if (m_state != TcpClientState::Ready) {
        postFailure({requestId, method, "not_ready", "Соединение с сервером ещё не готово.", "none"});
        return requestId;
    }
    const auto encoded = ProtocolCodec::encode(ProtocolRequest{requestId, method, params});
    if (const auto* error = std::get_if<ProtocolDecodeError>(&encoded)) {
        postFailure({requestId, method, error->code, "Запрос не соответствует протоколу или превышает размер кадра.", "none"});
        return requestId;
    }
    m_pending.emplace(requestId, PendingRequest{method, readOnly(method),
        std::chrono::steady_clock::now() + m_settings.responseTimeout});
    scheduleRequestTimeout();
    const auto generation = m_generation;
    if (!writeBytes(std::get<QByteArray>(encoded)) && m_generation == generation) {
        endSession("connection_error", "Не удалось отправить запрос. Перед повтором перечитайте данные.", true, true);
    }

    return requestId;
}

bool TcpClient::writeBytes(const QByteArray& bytes)
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState && m_socket->write(bytes) == bytes.size();
}

void TcpClient::readSocket(quint64 generation)
{
    while (m_generation == generation && m_socket && m_socket->bytesAvailable() > 0) {
        const auto batch = m_decoder.append(m_socket->read(kReadChunkBytes));
        for (const auto& frame : batch.frames) {
            processFrame(frame, generation);
            if (m_generation != generation) { return; }
        }
        if (batch.invalidFrame) {
            endSession("invalid_frame", "Сервер прислал недопустимый кадр.", false, true);
            return;
        }
    }
}

void TcpClient::processFrame(const QByteArray& frame, quint64 generation)
{
    const auto decoded = ProtocolCodec::decode(frame);
    const auto* message = std::get_if<ProtocolMessage>(&decoded);
    if (!message) {
        endSession("invalid_frame", "Сервер прислал неверное сообщение.", false, true);
        return;
    }
    if (const auto* success = std::get_if<ProtocolSuccess>(message)) { processSuccess(*success, generation); return; }
    if (const auto* failure = std::get_if<ProtocolFailure>(message)) { processFailure(*failure); return; }
    if (const auto* event = std::get_if<ProtocolEvent>(message)) {
        if (!m_info || (m_state != TcpClientState::Ready && m_state != TcpClientState::Synchronizing)
            || event->serverInstanceId != m_info->instanceId) {
            endSession("invalid_event", "Событие не принадлежит текущей авторизованной сессии.", false, true);
            return;
        }
        if (m_seenEvents.insert(event->eventId).second) { emit eventReceived(*event); }
        return;
    }
    endSession("protocol_error", "Сервер остановил сессию или нарушил протокол.", false, true);
}

void TcpClient::processSuccess(const ProtocolSuccess& response, quint64 generation)
{
    if (m_state == TcpClientState::Authenticating && response.requestId == m_authRequest) {
        const auto info = authInfo(response.result);
        if (!info) {
            endSession("incompatible_server", "Сервер объявил несовместимые параметры сессии.", false, true);
            return;
        }
        m_info = info;
        m_phaseTimer.start(static_cast<int>(m_settings.responseTimeout.count()));
        setState(TcpClientState::Synchronizing);
        if (m_generation != generation) { return; }
        const auto bytes = std::get<QByteArray>(ProtocolCodec::encode(ProtocolRequest{m_syncRequest, "get_all", {}}));
        if (!writeBytes(bytes) && m_generation == generation) {
            endSession("connection_error", "Не удалось запросить начальные данные.", true, true);
        }
        return;
    }
    if (m_state == TcpClientState::Synchronizing && response.requestId == m_syncRequest) {
        if (!m_info || !validSnapshot(response.result, *m_info)) {
            endSession("invalid_snapshot", "Сервер прислал неверный начальный снимок.", false, true);
            return;
        }
        m_snapshot = response.result;
        m_phaseTimer.stop();
        m_attempts = 0;
        setState(TcpClientState::Ready);
        if (m_generation == generation && m_state == TcpClientState::Ready) { emit ready(*m_info, m_snapshot); }
        return;
    }
    const auto found = m_pending.find(response.requestId);
    if (m_state != TcpClientState::Ready || found == m_pending.end()) { return; }
    m_pending.erase(found);
    scheduleRequestTimeout();
    emit responseReceived(response);
}

void TcpClient::processFailure(const ProtocolFailure& failure)
{
    if (m_state == TcpClientState::Authenticating && failure.requestId == m_authRequest) {
        endSession(failure.code, "Сервер отклонил авторизацию. Проверьте настройки подключения.", false, true);
        return;
    }
    if (m_state == TcpClientState::Synchronizing && failure.requestId == m_syncRequest) {
        endSession(failure.code, "Сервер не смог передать начальные данные.", false, true);
        return;
    }
    const auto found = m_pending.find(failure.requestId);
    if (m_state != TcpClientState::Ready || found == m_pending.end()) { return; }
    m_pending.erase(found);
    scheduleRequestTimeout();
    emit serverRejected(failure);
}

void TcpClient::detachSocket()
{
    if (!m_socket) { return; }
    auto* socket = m_socket.data();
    m_socket.clear();
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
}

void TcpClient::endSession(const QString& code, const QString& message, bool retry, bool report)
{
    const auto generation = ++m_generation;
    m_retryTimer.stop();
    m_phaseTimer.stop();
    m_requestTimer.stop();
    detachSocket();
    m_decoder.reset();
    m_info.reset();
    m_snapshot = {};
    m_seenEvents.clear();
    const auto pending = std::move(m_pending);
    m_pending.clear();
    for (const auto& [requestId, request] : pending) {
        postFailure({requestId, request.method, code, message, request.readOnly ? "none" : "unknown"});
    }
    const bool trying = retry && m_enabled;
    const bool exhausted = trying && m_attempts >= kMaximumConnectionAttempts;
    if (!trying) { m_enabled = false; }
    const auto delay = retryDelay();
    if (trying && !exhausted) { m_retryTimer.start(delay); }
    setState(exhausted ? TcpClientState::Exhausted
        : trying ? TcpClientState::WaitingToReconnect : TcpClientState::Disconnected);
    if (m_generation != generation) { return; }
    emit sessionReset();
    if (m_generation != generation) { return; }
    if (report) { emit connectionError(code, message); }
    if (m_generation != generation) { return; }
    if (exhausted) { emit retryExhausted(); }
    else if (trying) { emit retryScheduled(m_attempts + 1, delay); }
}

int TcpClient::retryDelay() const
{
    auto delay = m_settings.initialReconnectDelay.count();
    for (int attempt = 1; attempt < m_attempts; ++attempt) {
        delay = std::min(delay * 2, m_settings.maximumReconnectDelay.count());
    }

    return static_cast<int>(delay);
}

void TcpClient::postFailure(TcpClientRequestFailure failure)
{
    QTimer::singleShot(0, this, [this, failure = std::move(failure)]() { emit requestFailed(failure); });
}

void TcpClient::setState(TcpClientState state)
{
    if (m_state == state) { return; }
    m_state = state;
    emit stateChanged(state);
}

void TcpClient::scheduleRequestTimeout()
{
    m_requestTimer.stop();
    if (m_pending.empty()) { return; }
    const auto earliest = std::ranges::min_element(m_pending, [](const auto& left, const auto& right) {
        return left.second.deadline < right.second.deadline;
    })->second.deadline;
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(earliest - std::chrono::steady_clock::now()).count();
    m_requestTimer.start(static_cast<int>(std::clamp<std::int64_t>(remaining, 1, std::numeric_limits<int>::max())));
}

void TcpClient::onRetry()
{
    if (m_enabled && m_state == TcpClientState::WaitingToReconnect) { beginAttempt(); }
}

void TcpClient::onPhaseTimeout()
{
    endSession("connection_timeout", "Сервер не ответил вовремя при подключении.", true, true);
}

void TcpClient::onRequestTimeout()
{
    const auto now = std::chrono::steady_clock::now();
    const bool expired = std::ranges::any_of(m_pending, [now](const auto& entry) { return entry.second.deadline <= now; });
    if (!expired) { scheduleRequestTimeout(); return; }
    endSession("request_timeout", "Срок ответа истёк. Результат отправленной записи неизвестен.", true, true);
}

TcpClientState TcpClient::state() const { return m_state; }
int TcpClient::connectionAttempts() const { return m_attempts; }
std::optional<TcpClientServerInfo> TcpClient::serverInfo() const { return m_info; }
QJsonObject TcpClient::initialSnapshot() const { return m_snapshot; }

} // namespace msa::infra

