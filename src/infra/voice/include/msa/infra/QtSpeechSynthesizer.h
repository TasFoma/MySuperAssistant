#pragma once

#include "msa/core/SpeechSynthesizer.h"

#include <QAudioDevice>
#include <QObject>
#include <QString>
#include <QTextToSpeech>

#include <memory>

namespace msa::infra
{
/// @brief Ленивый адаптер Qt TextToSpeech; без PowerShell и запуска внешних команд.
class QtSpeechSynthesizer final : public QObject, public core::SpeechSynthesizer
{
public:
    /// @brief Пустое имя выбирает системный движок; явное имя позволяет проверить сбой плагина.
    explicit QtSpeechSynthesizer(QString engine = {}, QObject* parent = nullptr);
    ~QtSpeechSynthesizer() override;
    void setHandler(core::SpeechHandler handler) override;
    void speak(const std::string& text) override;
    void stop() override;
    /// @brief Смена вывода останавливает речь; системный выбор сохраняет штатный движок.
    void setOutputDevice(const QAudioDevice& device, bool systemDefault);

private:
    void initialize();
    void ready();
    void notify(core::SpeechState state, const QString& error = {});

private:
    QString m_engineName{};
    std::unique_ptr<QTextToSpeech> m_engine{};
    core::SpeechHandler m_handler{};
    QString m_pending{};
    bool m_requested{false};
    bool m_playing{false};
    QAudioDevice m_outputDevice{};
    bool m_systemDefault{true};
    bool m_outputConfigured{false};
};
} // namespace msa::infra
