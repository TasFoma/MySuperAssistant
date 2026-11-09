#pragma once

#include "msa/infra/VoskDecoder.h"

#include <QAudioSource>
#include <QByteArray>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

namespace msa::infra
{
/// @brief Один сеанс захвата и распознавания внутри изолированного процесса.
class VoskAudioSession final : public QObject
{
    Q_OBJECT
public:
    explicit VoskAudioSession(QObject* parent = nullptr);
    ~VoskAudioSession() override;
    /// @brief Загружает модель в потоке служебного процесса, без открытия микрофона.
    void load(const QString& path);
    /// @brief Выбирает микрофон по непрозрачному ID; пустой ID означает системное устройство.
    void setInputDevice(const QByteArray& identifier);
    /// @brief Начинает запись PCM 16 кГц/int16/mono; ошибки сообщаются сигналом failed.
    void start();
    /// @brief Завершает запись с накопленным текстом либо отменяет её без текста.
    void finish();
    void stop();
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] bool recording() const;
signals:
    void textReady(QString text);    ///< @brief Первая непустая фраза или FinalResult.
    void completed();                ///< @brief Устройство остановлено, можно начать новую запись.
    void failed(QString message);    ///< @brief Устройство остановлено после ошибки.
    void levelChanged(double level); ///< @brief Пиковый уровень PCM, без сохранения звука.
private:
    void readAudio();
    void complete(const QString& text);
    void fail(const QString& message);

private:
    std::unique_ptr<VoskDecoder> m_decoder{};
    QPointer<QAudioSource> m_audio{};
    QPointer<QIODevice> m_input{};
    QByteArray m_pending{};
    QByteArray m_inputDevice{};
    QElapsedTimer m_levelTimer{};
    bool m_recording{false};
};
} // namespace msa::infra
