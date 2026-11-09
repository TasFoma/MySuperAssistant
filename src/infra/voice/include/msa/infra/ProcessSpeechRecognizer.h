#pragma once

#include "msa/core/SpeechRecognizer.h"

#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>

#include <chrono>

namespace msa::infra
{
/// @brief Сроки независимого процесса; короткие значения передаются тестами.
struct RecognitionTimeouts
{
    std::chrono::milliseconds loading{std::chrono::seconds{120}};
    std::chrono::milliseconds recording{std::chrono::seconds{30}};
    std::chrono::milliseconds finishing{std::chrono::seconds{2}};
};
/// @brief Запускает C++ VoiceWorker и управляет записью без блокировки потока UI.
class ProcessSpeechRecognizer final : public QObject, public core::SpeechRecognizer
{
    Q_OBJECT
public:
    /// @brief Явный путь к вспомогательному executable, без Python и shell-команд.
    explicit ProcessSpeechRecognizer(QString executable, RecognitionTimeouts timeouts = {}, QObject* parent = nullptr);
    ~ProcessSpeechRecognizer() override;
    void setHandler(core::RecognitionHandler handler) override;
    void configure(const std::string& modelPath) override;
    void start() override;
    void finish() override;
    void cancel() override;
    void shutdown() override;
    [[nodiscard]] core::RecognitionState state() const override;

private:
    void launch();
    void stopWorker();
    void beginRecording();
    void receive(quint64 generation);
    void handle(const QJsonObject& object);
    void send(QJsonObject object);
    void fail(const QString& message);
    void setState(core::RecognitionState state);
    void notify(core::RecognitionEvent::Kind kind, const QString& text = {});

private:
    QString m_executable;
    RecognitionTimeouts m_timeouts;
    QString m_modelPath{};
    core::RecognitionHandler m_handler{};
    core::RecognitionState m_state{core::RecognitionState::Unconfigured};
    QLocalServer m_listener{this};
    QPointer<QLocalSocket> m_socket{};
    QPointer<QProcess> m_process{};
    QTimer m_deadline{this};
    QByteArray m_pending{};
    quint64 m_generation{};
    quint64 m_nextSession{};
    quint64 m_session{};
    bool m_startPending{false};
    bool m_hasResult{false};
    bool m_recordingTimedOut{false};
};
} // namespace msa::infra
