#pragma once

#include "msa/infra/VoskAudioSession.h"

#include <QByteArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QString>

namespace msa::desktop
{
/// @brief Отдельный C++-процесс: модель Vosk, захват PCM и одна фраза за запись.
class VoiceWorker final : public QObject
{
public:
    explicit VoiceWorker(QObject* parent = nullptr);
    /// @brief Соединяется с закрытым локальным каналом родительского процесса.
    void connectTo(const QString& channel);

private:
    void receive();
    void handle(const QJsonObject& object);
    void send(QJsonObject object);
    void fail(const QString& message);

private:
    QLocalSocket m_socket{this};
    QByteArray m_pending{};
    infra::VoskAudioSession m_capture{this};
    quint64 m_session{};
};
} // namespace msa::desktop
