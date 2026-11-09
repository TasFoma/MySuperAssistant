#pragma once

#include "msa/core/ClientData.h"
#include "msa/infra/TcpClient.h"
#include "msa/ui/ClientModels.h"

#include <QObject>
#include <QString>
#include <cstdint>
#include <map>
#include <optional>

namespace msa::desktop
{
/// @brief Связывает типизированные модели и транспорт; формат JSON остаётся в infra.
class DesktopClientController final : public QObject
{
public:
    /// @brief Объекты невладеющие, должны жить дольше связки в том же потоке Qt.
    DesktopClientController(infra::TcpClient& client, ui::ClientModels& models, QObject* parent = nullptr);

private:
    struct Context
    {
        std::optional<core::ClientReadRequest> read{};
        std::optional<core::ClientAction> action{};
        std::uint64_t session{};
        std::uint64_t view{};
        std::uint64_t sequence{};
        QString key{};
    };
    void read(core::ClientReadRequest request, std::uint64_t session, std::uint64_t view);
    void act(const core::ClientAction& action);
    void response(const infra::ProtocolSuccess& response);
    void queueRefresh();
    void updateState();

    infra::TcpClient& m_client;
    ui::ClientModels& m_models;
    std::map<QString, Context> m_pending{};
    std::map<QString, std::uint64_t> m_sequences{};
    int m_attempt{0};
    int m_delay{0};
    bool m_refreshQueued{false};
};
} // namespace msa::desktop
