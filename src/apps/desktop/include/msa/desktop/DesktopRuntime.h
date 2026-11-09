#pragma once

#include "msa/core/ClientPreferences.h"
#include "msa/desktop/AudioDevicesController.h"
#include "msa/desktop/DesktopClientController.h"
#include "msa/desktop/DesktopPreferencesController.h"
#include "msa/desktop/MusicController.h"
#include "msa/desktop/SpeechController.h"
#include "msa/desktop/VoiceController.h"
#include "msa/infra/AudioDevices.h"
#include "msa/infra/LocalMusicPlayer.h"
#include "msa/infra/ProcessSpeechRecognizer.h"
#include "msa/infra/QtSpeechSynthesizer.h"
#include "msa/infra/TcpClient.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/DesktopState.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>

namespace msa::desktop
{
/// @brief Параметры сборки клиента; тесты передают временные файлы и короткие сроки.
struct DesktopRuntimeOptions
{
    QString clientSettingsPath{};
    QString musicSettingsPath{};
    std::uint32_t musicSeed{};
    std::chrono::milliseconds connectTimeout{std::chrono::seconds{10}};
    std::chrono::milliseconds responseTimeout{std::chrono::seconds{180}};
    std::chrono::milliseconds initialReconnectDelay{std::chrono::seconds{1}};
    std::chrono::milliseconds maximumReconnectDelay{std::chrono::seconds{15}};
    QString voiceSettingsPath{};
    QString voiceWorkerPath{};
    QString audioSettingsPath{};
};
/// @brief Владеет моделями и локальной музыкой, создаёт сетевую сессию по сохранённым параметрам.
class DesktopRuntime final : public QObject
{
public:
    /// @brief Конструктор соединяет объекты без чтения файлов и подключения.
    explicit DesktopRuntime(DesktopRuntimeOptions options, QObject* parent = nullptr);
    ~DesktopRuntime() override;
    /// @brief Восстанавливает музыку без автозапуска и применяет auto_connect один раз.
    void start();
    /// @brief Завершает сетевую сессию и отменяет повторы перед закрытием окна.
    void stop();
    /// @brief Стабильные объекты, передаваемые обязательными свойствами QML-окна.
    [[nodiscard]] ui::ClientModels* models();
    [[nodiscard]] ui::DesktopState* state();

private:
    void connectSaved();
    void stopSession();

private:
    DesktopRuntimeOptions m_options;
    ui::ClientModels m_models{this};
    ui::DesktopState m_state{m_models, this};
    DesktopPreferencesController m_preferences;
    infra::LocalMusicPlayer m_player;
    MusicController m_music;
    infra::ProcessSpeechRecognizer m_recognizer;
    infra::QtSpeechSynthesizer m_synthesizer;
    SpeechController m_speech;
    VoiceController m_voice;
    infra::AudioDevices m_audioDevices;
    AudioDevicesController m_audioController;
    std::optional<infra::TcpClient> m_client{};
    std::optional<DesktopClientController> m_controller{};
    std::optional<core::ClientPreferences> m_activePreferences{};
    bool m_started{false};
    bool m_stopped{false};
};
} // namespace msa::desktop
