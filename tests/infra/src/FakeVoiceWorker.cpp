#include "msa/infra/VoiceProtocol.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

#include <utility>

namespace
{
constexpr int kFragmentDelayMilliseconds = 5;
constexpr int kLateMilliseconds = 150;
class FakeVoiceWorker final : public QObject
{
public:
    explicit FakeVoiceWorker(const QString& channel);

private:
    void receive();
    void send(QJsonObject object);
    void result(quint64 session);

private:
    QLocalSocket m_socket{this};
    QByteArray m_pending{};
    QString m_scenario{};
};
FakeVoiceWorker::FakeVoiceWorker(const QString& channel)
{
    connect(&m_socket, &QLocalSocket::disconnected, this, [] { QCoreApplication::quit(); });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] { receive(); });
    m_socket.connectToServer(channel);
}
void FakeVoiceWorker::send(QJsonObject object)
{
    static_cast<void>(m_socket.write(msa::infra::VoiceProtocol::encode(std::move(object))));
}
void FakeVoiceWorker::result(quint64 session)
{
    const auto bytes = msa::infra::VoiceProtocol::encode(
        {{"type", "text"}, {"session", QString::number(session)}, {"text", "  Распознанная фраза 🙂  "}});
    const auto split = bytes.indexOf(QString::fromUtf8("🙂").toUtf8()) + 1;
    static_cast<void>(m_socket.write(bytes.first(split)));
    QTimer::singleShot(kFragmentDelayMilliseconds, this,
                       [this, bytes, split, session]
                       {
                           static_cast<void>(m_socket.write(bytes.sliced(split)));
                           send({{"type", "done"}, {"session", QString::number(session)}});
                       });
}
void FakeVoiceWorker::receive()
{
    const auto messages = msa::infra::VoiceProtocol::consume(m_pending, m_socket.readAll());
    for (const auto& message : messages)
    {
        const auto type = message.value("type").toString();
        if (type == "load")
        {
            m_scenario = QFileInfo{message.value("model").toString()}.fileName();
            if (m_scenario == "loading_timeout")
            {
                continue;
            }
            if (m_scenario == "bad_json")
            {
                static_cast<void>(m_socket.write("{broken\n"));
                continue;
            }
            if (m_scenario == "oversize")
            {
                static_cast<void>(m_socket.write(QByteArray(msa::infra::VoiceProtocol::kMaximumFrameBytes + 2, 'x')));
                continue;
            }
            send({{"type", "ready"}});
        }
        else if (type == "start")
        {
            const auto session = msa::infra::VoiceProtocol::session(message);
            if (m_scenario == "crash")
            {
                QCoreApplication::exit(9);
                return;
            }
            if (m_scenario == "finish" || m_scenario == "never_finish")
            {
                continue;
            }
            if (m_scenario == "late")
            {
                QTimer::singleShot(kLateMilliseconds, this, [this, session] { result(session); });
                continue;
            }
            if (m_scenario == "wrong_session")
            {
                send({{"type", "text"}, {"session", QString::number(session + 1)}, {"text", "Чужой ответ"}});
            }
            result(session);
        }
        else if (type == "finish" && m_scenario != "never_finish")
        {
            result(msa::infra::VoiceProtocol::session(message));
        }
    }
}
} // namespace
int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QCommandLineParser parser;
    const QCommandLineOption channel{"channel", "channel", "name"};
    parser.addOption(channel);
    parser.process(application);
    FakeVoiceWorker worker{parser.value(channel)};

    return application.exec();
}
