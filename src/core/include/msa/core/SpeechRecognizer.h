#pragma once

#include <functional>
#include <string>

namespace msa::core
{
/// @brief Состояние локального распознавания, независимое от серверной сессии.
enum class RecognitionState
{
    Unconfigured,
    Idle,
    Loading,
    Ready,
    Recording,
    Finishing,
    Failed
};
/// @brief Событие распознавателя; текст выдаётся только как завершённая фраза.
struct RecognitionEvent
{
    enum class Kind
    {
        State,
        Text,
        Level,
        Error
    };
    Kind kind{Kind::State};
    RecognitionState state{RecognitionState::Unconfigured};
    std::string text{};
    double level{}; ///< @brief Пиковый уровень входного PCM от 0 до 1 для события Level.
};
using RecognitionHandler = std::function<void(const RecognitionEvent&)>;
/// @brief Явные действия записи из UI без зависимости от сети.
enum class RecognitionCommand
{
    Start,
    Finish,
    Cancel
};

/// @brief Асинхронное локальное распознавание; вызовы и обработчик выполняются в потоке владельца.
class SpeechRecognizer
{
public:
    virtual ~SpeechRecognizer();
    /// @brief Заменяет единственный обработчик; пустой обработчик отписывает потребителя.
    virtual void setHandler(RecognitionHandler handler) = 0;
    /// @brief Настраивает абсолютный путь модели без запуска записи или загрузки.
    virtual void configure(const std::string& modelPath) = 0;
    /// @brief Загружает модель при необходимости, затем начинает одну запись.
    virtual void start() = 0;
    /// @brief Заканчивает запись и возвращает накопленный финальный текст.
    virtual void finish() = 0;
    /// @brief Отменяет загрузку/запись с отбрасыванием текста; следующий start явный.
    virtual void cancel() = 0;
    /// @brief Останавливает вспомогательный процесс и отменяет таймеры.
    virtual void shutdown() = 0;
    /// @brief Текущее состояние без операций ввода-вывода.
    [[nodiscard]] virtual RecognitionState state() const = 0;
};
} // namespace msa::core
