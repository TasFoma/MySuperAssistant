#pragma once

#include <QByteArray>
#include <QString>
#include <vosk_api.h>

#include <memory>
#include <optional>

namespace msa::infra
{
/// @brief Нативный C API Vosk; используется только внутри отдельного VoiceWorker.
class VoskDecoder final
{
public:
    /// @brief Загружает одну модель; длительная операция изолирована процессом.
    explicit VoskDecoder(const QString& modelPath);
    /// @brief Создаёт новое распознавание PCM int16 mono 16 кГц на той же модели.
    void begin();
    /// @brief Первая законченная непустая фраза, иначе nullopt; PCM содержит целые сэмплы.
    [[nodiscard]] std::optional<QString> accept(const QByteArray& pcm);
    /// @brief Финальная накопленная фраза при ручном завершении.
    [[nodiscard]] QString finish();

private:
    [[nodiscard]] static QString text(const char* json);

private:
    std::unique_ptr<VoskModel, decltype(&vosk_model_free)> m_model{nullptr, &vosk_model_free};
    std::unique_ptr<VoskRecognizer, decltype(&vosk_recognizer_free)> m_recognizer{nullptr, &vosk_recognizer_free};
};
} // namespace msa::infra
