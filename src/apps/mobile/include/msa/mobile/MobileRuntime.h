#pragma once

#include "msa/client/ClientController.h"
#include "msa/client/SportController.h"
#include "msa/core/ClientPreferences.h"
#include "msa/infra/TcpClient.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/MobileState.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <optional>

namespace msa::mobile
{
/// @brief Путь настроек и сроки; тесты используют временный файл и локальный сервер.
struct MobileRuntimeOptions
{
    QString clientSettingsPath{};
    std::chrono::milliseconds connectTimeout{std::chrono::seconds{10}};
    std::chrono::milliseconds responseTimeout{std::chrono::seconds{180}};
    std::chrono::milliseconds initialReconnectDelay{std::chrono::seconds{1}};
    std::chrono::milliseconds maximumReconnectDelay{std::chrono::seconds{15}};
    QString sportDatabasePath{};
};
/// @brief Соединяет мобильные модели, постоянные настройки и заменяемую TCP-сессию.
class MobileRuntime final : public QObject
{
public:
    /// @brief Создаёт объекты без чтения файла и подключения.
    explicit MobileRuntime(MobileRuntimeOptions options, QObject* parent = nullptr);
    ~MobileRuntime() override;
    /// @brief Один раз читает настройки и применяет auto_connect после создания окна.
    void start();
    /// @brief Завершает запросы и отменяет повторы; повторный вызов безопасен.
    void stop();
    /// @brief Стабильные объекты для обязательных свойств окна.
    [[nodiscard]] ui::ClientModels* models();
    [[nodiscard]] ui::MobileState* state();
    /// @brief client.settings.json в AppConfigLocation приложения msa_mobile.
    [[nodiscard]] static QString defaultSettingsPath();

private:
    void loadPreferences();
    void savePreferences(const core::ClientPreferences& preferences);
    void connectSaved();
    void stopSession();

private:
    MobileRuntimeOptions m_options;
    ui::ClientModels m_models{this};
    ui::MobileState m_state{m_models, this};
    std::optional<infra::TcpClient> m_client{};
    std::optional<client::ClientController> m_controller{};
    std::optional<core::ClientPreferences> m_activePreferences{};
    client::SportController m_sport{*m_state.sport(), m_options.sportDatabasePath, this};
    bool m_started{false};
    bool m_stopped{false};
};
} // namespace msa::mobile
