#include "msa/infra/ProcessSpeechRecognizer.h"

#include "msa/infra/VoiceProtocol.h"

#include <QDir>
#include <QUuid>

#include <limits>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{
constexpr int kShutdownMilliseconds = 2000;
bool validTimeout(std::chrono::milliseconds value)
{
    return value.count() > 0 && value.count() <= std::numeric_limits<int>::max();
}
} // namespace
ProcessSpeechRecognizer::ProcessSpeechRecognizer(QString executable, RecognitionTimeouts timeouts, QObject* parent) :
    QObject{parent}, m_executable{std::move(executable)}, m_timeouts{timeouts}
{
    if (m_executable.isEmpty() || !validTimeout(timeouts.loading) || !validTimeout(timeouts.recording) ||
        !validTimeout(timeouts.finishing))
    {
        throw std::invalid_argument{"Неверные параметры распознавания."};
    }
    m_deadline.setSingleShot(true);
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_deadline, &QTimer::timeout, this,
            [this]
            {
                if (m_state == core::RecognitionState::Recording)
                {
                    m_recordingTimedOut = true;
                    finish();
                }
                else
                {
                    fail("Процесс распознавания не ответил вовремя. Повторите действие явно.");
                }
            });
    m_listener.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_listener, &QLocalServer::newConnection, this,
            [this]
            {
                auto* socket = m_listener.nextPendingConnection();
                if (!socket)
                {
                    return;
                }
                if (m_socket || m_state != core::RecognitionState::Loading)
                {
                    socket->abort();
                    socket->deleteLater();
                    return;
                }
                m_socket = socket;
                m_listener.close();
                const auto generation = m_generation;
                connect(socket, &QLocalSocket::readyRead, this, [this, generation] { receive(generation); });
                connect(socket, &QLocalSocket::disconnected, this,
                        [this, generation]
                        {
                            receive(generation);
                            if (generation == m_generation)
                            {
                                fail("Процесс распознавания отключился.");
                            }
                        });
                send({{"type", "load"}, {"model", m_modelPath}, {"input_device", m_inputDevice}});
            });
}
ProcessSpeechRecognizer::~ProcessSpeechRecognizer()
{
    m_handler = {};
    stopWorker();
    // При закрытии приложения дожидаемся уже убитых дочерних процессов, не модели.
    for (auto* process : findChildren<QProcess*>(QString{}, Qt::FindDirectChildrenOnly))
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
            static_cast<void>(process->waitForFinished(kShutdownMilliseconds));
        }
    }
}
void ProcessSpeechRecognizer::setHandler(core::RecognitionHandler handler)
{
    m_handler = std::move(handler);
}
core::RecognitionState ProcessSpeechRecognizer::state() const
{
    return m_state;
}
void ProcessSpeechRecognizer::notify(core::RecognitionEvent::Kind kind, const QString& text, double level)
{
    const auto handler = m_handler;
    if (handler)
    {
        handler({kind, m_state, text.toStdString(), level});
    }
}
void ProcessSpeechRecognizer::setState(core::RecognitionState state)
{
    if (m_state == state)
    {
        return;
    }
    m_state = state;
    notify(core::RecognitionEvent::Kind::State);
}
void ProcessSpeechRecognizer::configure(const std::string& modelPath)
{
    const auto path = QString::fromStdString(modelPath);
    if (path == m_modelPath)
    {
        return;
    }
    stopWorker();
    m_modelPath = path;
    if (!path.isEmpty() && (!QDir::isAbsolutePath(path) || path.contains(QChar::Null)))
    {
        m_modelPath.clear();
        fail("Укажите абсолютный путь к папке модели Vosk.");
        return;
    }
    setState(path.isEmpty() ? core::RecognitionState::Unconfigured : core::RecognitionState::Idle);
}
void ProcessSpeechRecognizer::setInputDevice(const QString& identifier)
{
    if (m_inputDevice == identifier)
    {
        return;
    }
    m_inputDevice = identifier;
    shutdown();
}
void ProcessSpeechRecognizer::start()
{
    using State = core::RecognitionState;
    if (m_state == State::Loading || m_state == State::Recording || m_state == State::Finishing)
    {
        return;
    }
    if (m_modelPath.isEmpty())
    {
        fail("Сначала укажите папку модели Vosk.");
        return;
    }
    if (m_state == State::Ready)
    {
        beginRecording();
        return;
    }
    m_startPending = true;
    launch();
}
void ProcessSpeechRecognizer::launch()
{
    stopWorker();
    m_startPending = true;
    const auto channel = "msa-voice-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!m_listener.listen(channel))
    {
        fail("Не удалось открыть канал распознавания.");
        return;
    }
    const auto generation = m_generation;
    m_process = new QProcess{this};
    auto* process = m_process.data();
    connect(process, &QProcess::readyReadStandardError, process,
            [process] { static_cast<void>(process->readAllStandardError()); });
    connect(process, &QProcess::readyReadStandardOutput, process,
            [process] { static_cast<void>(process->readAllStandardOutput()); });
    connect(process, &QProcess::errorOccurred, this,
            [this, generation](QProcess::ProcessError)
            {
                if (generation == m_generation)
                {
                    fail("Не удалось запустить процесс распознавания.");
                }
            });
    connect(process, &QProcess::finished, this,
            [this, generation](int, QProcess::ExitStatus)
            {
                receive(generation);
                if (generation == m_generation)
                {
                    fail("Процесс распознавания завершился. Повторите действие явно.");
                }
            });
    setState(core::RecognitionState::Loading);
    if (generation != m_generation)
    {
        return;
    }
    m_deadline.start(m_timeouts.loading);
    process->start(m_executable, {"--channel", channel});
}
void ProcessSpeechRecognizer::stopWorker()
{
    ++m_generation;
    m_deadline.stop();
    m_listener.close();
    m_pending.clear();
    m_startPending = false;
    m_session = 0;
    m_recordingTimedOut = false;
    if (m_socket)
    {
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket.clear();
    }
    if (m_process)
    {
        auto* process = m_process.data();
        m_process.clear();
        disconnect(process, nullptr, this, nullptr);
        if (process->state() == QProcess::NotRunning)
        {
            process->deleteLater();
        }
        else
        {
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            process->kill();
        }
    }
}
void ProcessSpeechRecognizer::shutdown()
{
    stopWorker();
    setState(m_modelPath.isEmpty() ? core::RecognitionState::Unconfigured : core::RecognitionState::Idle);
}
void ProcessSpeechRecognizer::cancel()
{
    shutdown();
}
void ProcessSpeechRecognizer::finish()
{
    if (m_state == core::RecognitionState::Loading)
    {
        cancel();
        return;
    }
    if (m_state != core::RecognitionState::Recording)
    {
        return;
    }
    const auto generation = m_generation;
    m_deadline.start(m_timeouts.finishing);
    send({{"type", "finish"}, {"session", QString::number(m_session)}});
    if (generation != m_generation)
    {
        return;
    }
    setState(core::RecognitionState::Finishing);
}
void ProcessSpeechRecognizer::beginRecording()
{
    if (m_nextSession == std::numeric_limits<quint64>::max())
    {
        fail("Исчерпан счётчик записей.");
        return;
    }
    m_session = ++m_nextSession;
    m_hasResult = false;
    m_recordingTimedOut = false;
    m_startPending = false;
    const auto generation = m_generation;
    m_deadline.start(m_timeouts.recording);
    send({{"type", "start"}, {"session", QString::number(m_session)}});
    if (generation != m_generation)
    {
        return;
    }
    setState(core::RecognitionState::Recording);
}
void ProcessSpeechRecognizer::send(QJsonObject object)
{
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState)
    {
        fail("Канал распознавания недоступен.");
        return;
    }
    try
    {
        const auto bytes = VoiceProtocol::encode(std::move(object));
        if (m_socket->write(bytes) != bytes.size())
        {
            fail("Не удалось передать команду распознавания.");
        }
    }
    catch (const std::exception&)
    {
        fail("Команда распознавания слишком велика или некорректна.");
    }
}
void ProcessSpeechRecognizer::receive(quint64 generation)
{
    if (generation != m_generation || !m_socket)
    {
        return;
    }
    try
    {
        const auto messages = VoiceProtocol::consume(m_pending, m_socket->readAll());
        for (const auto& message : messages)
        {
            if (generation != m_generation)
            {
                return;
            }
            handle(message);
        }
    }
    catch (const std::exception&)
    {
        fail("Неверный ответ процесса распознавания.");
    }
}
void ProcessSpeechRecognizer::handle(const QJsonObject& object)
{
    const auto type = object.value("type").toString();
    if (type == "ready" && object.size() == 2 && m_state == core::RecognitionState::Loading)
    {
        m_deadline.stop();
        const auto startPending = m_startPending;
        setState(core::RecognitionState::Ready);
        if (startPending && m_state == core::RecognitionState::Ready)
        {
            beginRecording();
        }
        return;
    }
    if (type == "error" && object.size() == 3 && object.value("message").isString())
    {
        const auto message = object.value("message").toString().trimmed();
        if (message.isEmpty() || message.contains(QChar::Null))
        {
            throw std::runtime_error{"Неверная ошибка записи."};
        }
        fail(message);
        return;
    }
    if (VoiceProtocol::session(object) != m_session)
    {
        return;
    }
    if (type == "level" && object.size() == 4 && object.value("value").isDouble())
    {
        const auto level = object.value("value").toDouble(-1);
        if (!(level >= 0 && level <= 1))
        {
            throw std::runtime_error{"Неверный уровень микрофона."};
        }
        if (m_state == core::RecognitionState::Recording)
        {
            notify(core::RecognitionEvent::Kind::Level, {}, level);
        }
        return;
    }
    if (m_state != core::RecognitionState::Recording && m_state != core::RecognitionState::Finishing)
    {
        throw std::runtime_error{"Неожиданное состояние записи."};
    }
    if (type == "text" && object.size() == 4 && object.value("text").isString() && !m_hasResult)
    {
        const auto text = object.value("text").toString().trimmed();
        if (text.contains(QChar::Null))
        {
            throw std::runtime_error{"Неверный текст записи."};
        }
        if (!text.isEmpty())
        {
            m_hasResult = true;
            notify(core::RecognitionEvent::Kind::Text, text);
        }
        return;
    }
    if (type == "done" && object.size() == 3)
    {
        m_deadline.stop();
        m_session = 0;
        const auto timedOut = m_recordingTimedOut;
        const auto empty = !m_hasResult;
        const auto generation = m_generation;
        m_recordingTimedOut = false;
        setState(core::RecognitionState::Ready);
        if (empty && generation == m_generation && m_state == core::RecognitionState::Ready)
        {
            notify(core::RecognitionEvent::Kind::Error,
                   "Речь не распознана. Проверьте выбранный микрофон и индикатор уровня, затем повторите запись.");
            return;
        }
        if (timedOut && generation == m_generation && m_state == core::RecognitionState::Ready)
        {
            notify(core::RecognitionEvent::Kind::Error, "Запись завершена по ограничению времени.");
        }
        return;
    }
    throw std::runtime_error{"Неизвестное событие записи."};
}
void ProcessSpeechRecognizer::fail(const QString& message)
{
    stopWorker();
    const auto generation = m_generation;
    setState(core::RecognitionState::Failed);
    if (generation != m_generation)
    {
        return;
    }
    notify(core::RecognitionEvent::Kind::Error, message);
}
} // namespace msa::infra
