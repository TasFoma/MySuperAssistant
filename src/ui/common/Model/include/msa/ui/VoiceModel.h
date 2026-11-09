#pragma once

#include "msa/core/SpeechRecognizer.h"
#include "msa/core/SpeechSynthesizer.h"

#include <QObject>
#include <QString>

namespace msa::ui
{
/// @brief Локальная запись и черновик чата; аудио и файлы обрабатывает связка apps.
class VoiceModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath NOTIFY modelChanged)
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY statusChanged)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY statusChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY statusChanged)
    Q_PROPERTY(bool speaking READ speaking NOTIFY statusChanged)
public:
    explicit VoiceModel(QObject* parent = nullptr);
    /// @brief Применяет только успешно прочитанный или сохранённый путь.
    void applyModelPath(const QString& path);
    /// @brief События core обновляют статус и добавляют фразу в черновик без отправки.
    void applyEvent(const core::RecognitionEvent& event);
    /// @brief Обновляет состояние локальной озвучки без изменения черновика.
    void applySpeechEvent(const core::SpeechEvent& event);
    [[nodiscard]] bool speaking() const;
    /// @brief Независимая от сетевой ошибки диагностика голоса.
    void setError(const QString& message);
    [[nodiscard]] QString modelPath() const;
    [[nodiscard]] QString draft() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] bool canStart() const;
    [[nodiscard]] bool canFinish() const;
    [[nodiscard]] bool canCancel() const;
    /// @brief Сохраняет пользовательский текст в памяти между переключениями экранов.
    void setDraft(const QString& text);
    /// @brief Проверяет локальный путь или file URL из диалога и запрашивает сохранение.
    Q_INVOKABLE void chooseModel(const QString& path);
    /// @brief Явные действия микрофона; finish возвращает накопленное, cancel отбрасывает.
    Q_INVOKABLE void start();
    Q_INVOKABLE void finish();
    Q_INVOKABLE void cancel();
    /// @brief Озвучивает выбранное сообщение только по явному действию.
    Q_INVOKABLE void speak(const QString& text);
    /// @brief Запрашивает немедленную остановку озвучки.
    Q_INVOKABLE void stopSpeech();
signals:
    void modelChanged();                                          ///< @brief Изменён сохранённый путь.
    void draftChanged();                                          ///< @brief Изменён черновик; отправка отдельно.
    void statusChanged();                                         ///< @brief Изменён статус или ошибка записи.
    void saveRequested(QString path);                             ///< @brief Нормализованный абсолютный путь.
    void commandRequested(msa::core::RecognitionCommand command); ///< @brief Локальная команда связке apps.
    void speechRequested(QString text); ///< @brief Обычный текст сообщения для локального синтеза.
    void stopSpeechRequested();         ///< @brief Явная остановка озвучки.
private:
    QString m_modelPath{};
    QString m_draft{};
    QString m_error{};
    core::RecognitionState m_state{core::RecognitionState::Unconfigured};
    bool m_speaking{false};
};
} // namespace msa::ui
