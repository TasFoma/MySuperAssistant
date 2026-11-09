#pragma once

#include <functional>
#include <string>

namespace msa::core
{
/// @brief Состояние локальной озвучки, независимое от сети и распознавания.
enum class SpeechState
{
    Idle,
    Speaking,
    Failed
};
/// @brief Текущее состояние и диагностика; пустая ошибка означает успех.
struct SpeechEvent
{
    SpeechState state{SpeechState::Idle};
    std::string error{};
};
using SpeechHandler = std::function<void(const SpeechEvent&)>;
/// @brief Озвучка в потоке владельца; новая фраза заменяет предыдущую.
class SpeechSynthesizer
{
public:
    virtual ~SpeechSynthesizer();
    /// @brief Заменяет обработчик; пустой обработчик отписывает потребителя.
    virtual void setHandler(SpeechHandler handler) = 0;
    /// @brief Асинхронно произносит обычный UTF-8 текст без интерпретации команд.
    virtual void speak(const std::string& text) = 0;
    /// @brief Прерывает речь и отбрасывает ожидающий текст.
    virtual void stop() = 0;
};
} // namespace msa::core
