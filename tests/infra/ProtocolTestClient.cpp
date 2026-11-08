#include "ProtocolTestClient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>

#include <algorithm>
#include <stdexcept>

namespace msa::infra::test {
namespace {
constexpr int kWaitMs{6000};
constexpr int kEventSliceMs{5};
constexpr unsigned long kPollMs{1};
} // namespace

bool waitForTest(const std::function<bool()>& predicate)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < kWaitMs) {
        if (predicate()) { return true; }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kEventSliceMs);
        QThread::msleep(kPollMs);
    }

    return predicate();
}

ProtocolTestClient::ProtocolTestClient() = default;

void ProtocolTestClient::connectTo(quint16 port)
{
    m_socket.abort();
    m_frames.clear();
    m_pending.clear();
    m_requestNumber = 0;
    m_socket.connectToHost(QHostAddress::LocalHost, port);
    if (!waitForTest([this]() { return m_socket.state() == QAbstractSocket::ConnectedState; })) {
        throw std::runtime_error{"Тестовый клиент не подключился к серверу"};
    }
}

QJsonObject ProtocolTestClient::authenticate(const QString& token)
{
    sendObject({{"type", "auth"}, {"version", 1}, {"request_id", "auth"}, {"token", token}});

    return response("auth");
}

QJsonObject ProtocolTestClient::request(const QString& method, const QJsonObject& params, const QString& requestId)
{
    const auto correlation = requestId.isEmpty() ? QString{"request-%1"}.arg(++m_requestNumber) : requestId;
    sendRequest(correlation, method, params);

    return response(correlation);
}

void ProtocolTestClient::sendRequest(const QString& requestId, const QString& method, const QJsonObject& params)
{
    sendObject({{"type", "request"}, {"request_id", requestId}, {"method", method}, {"params", params}});
}

void ProtocolTestClient::sendObject(const QJsonObject& object)
{
    sendBytes(QJsonDocument{object}.toJson(QJsonDocument::Compact) + '\n');
}

void ProtocolTestClient::sendBytes(const QByteArray& bytes)
{
    if (m_socket.write(bytes) != bytes.size()) {
        throw std::runtime_error{"Не удалось отправить тестовый кадр"};
    }
    m_socket.flush();
    if (!waitForTest([this]() { return m_socket.bytesToWrite() == 0; })) {
        throw std::runtime_error{"Тестовый кадр не передан TCP-сокету"};
    }
}

QJsonObject ProtocolTestClient::response(const QString& requestId)
{
    const auto matches = [requestId](const QJsonObject& frame) {
        return frame.value("type") == "response" && frame.value("request_id").toString() == requestId;
    };
    if (!waitForTest([this, matches]() {
        collect();
        return std::ranges::any_of(m_pending, matches);
    })) {
        throw std::runtime_error{"Нет ответа на тестовый request_id: " + requestId.toStdString()};
    }
    const auto found = std::ranges::find_if(m_pending, matches);
    const auto result = *found;
    m_pending.erase(found);

    return result;
}

void ProtocolTestClient::collect()
{
    while (m_socket.canReadLine()) {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(m_socket.readLine(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            throw std::runtime_error{"Сервер отправил некорректный JSON-кадр"};
        }
        m_pending.push_back(document.object());
        m_frames.push_back(document.object());
    }
}

void ProtocolTestClient::disconnect()
{
    m_socket.abort();
}

bool ProtocolTestClient::waitDisconnected()
{
    return waitForTest([this]() { return m_socket.state() == QAbstractSocket::UnconnectedState; });
}

const std::vector<QJsonObject>& ProtocolTestClient::frames()
{
    collect();

    return m_frames;
}
} // namespace msa::infra::test

