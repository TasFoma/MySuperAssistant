#include "msa/desktop/VoiceWorker.h"

#include "msa/infra/VoiceProtocol.h"

#include <QCoreApplication>

#include <exception>
#include <stdexcept>
#include <utility>

namespace msa::desktop
{
VoiceWorker::VoiceWorker(QObject* parent) : QObject{parent}
{
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] { receive(); });
    connect(&m_socket, &QLocalSocket::disconnected, this,
            [this]
            {
                m_capture.stop();
                QCoreApplication::quit();
            });
    connect(&m_socket, &QLocalSocket::errorOccurred, this,
            [this](QLocalSocket::LocalSocketError)
            {
                m_capture.stop();
                QCoreApplication::exit(1);
            });
    connect(&m_capture, &infra::VoskAudioSession::textReady, this, [this](const QString& text)
            { send({{"type", "text"}, {"session", QString::number(m_session)}, {"text", text}}); });
    connect(&m_capture, &infra::VoskAudioSession::completed, this,
            [this] { send({{"type", "done"}, {"session", QString::number(m_session)}}); });
    connect(&m_capture, &infra::VoskAudioSession::failed, this, [this](const QString& message) { fail(message); });
}
void VoiceWorker::connectTo(const QString& channel)
{
    m_socket.connectToServer(channel);
}
void VoiceWorker::send(QJsonObject object)
{
    const auto bytes = infra::VoiceProtocol::encode(std::move(object));
    if (m_socket.write(bytes) != bytes.size())
    {
        QCoreApplication::exit(1);
    }
}
void VoiceWorker::receive()
{
    try
    {
        const auto messages = infra::VoiceProtocol::consume(m_pending, m_socket.readAll());
        for (const auto& message : messages)
        {
            handle(message);
        }
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
}
void VoiceWorker::handle(const QJsonObject& object)
{
    const auto type = object.value("type").toString();
    if (type == "load" && object.size() == 3 && !m_capture.loaded() && object.value("model").isString())
    {
        m_capture.load(object.value("model").toString());
        send({{"type", "ready"}});
        return;
    }
    if (!m_capture.loaded())
    {
        throw std::runtime_error{"Модель ещё не загружена."};
    }
    const auto session = infra::VoiceProtocol::session(object);
    if (object.size() != 3)
    {
        throw std::runtime_error{"Неверная команда записи."};
    }
    if (type == "start" && !m_capture.recording())
    {
        m_session = session;
        m_capture.start();
        return;
    }
    if (type == "finish" && session == m_session)
    {
        m_capture.finish();
        return;
    }
    throw std::runtime_error{"Неожиданная команда записи."};
}
void VoiceWorker::fail(const QString& message)
{
    m_capture.stop();
    send({{"type", "error"}, {"message", message}});
    m_socket.flush();
    // Родитель закроет канал после получения ошибки; ранний выход теряет диагностику.
}
} // namespace msa::desktop
