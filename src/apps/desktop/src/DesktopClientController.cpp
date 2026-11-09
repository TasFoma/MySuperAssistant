#include "msa/desktop/DesktopClientController.h"

#include "msa/infra/ClientProtocol.h"

#include <QTimer>
#include <exception>
#include <utility>
#include <variant>

namespace msa::desktop
{
namespace
{
core::ClientConnectionState state(infra::TcpClientState value)
{
    using Source = infra::TcpClientState;
    using Target = core::ClientConnectionState;
    switch (value)
    {
    case Source::Disconnected:
        return Target::Disconnected;
    case Source::Connecting:
        return Target::Connecting;
    case Source::Authenticating:
        return Target::Authenticating;
    case Source::Synchronizing:
        return Target::Synchronizing;
    case Source::Ready:
        return Target::Ready;
    case Source::WaitingToReconnect:
        return Target::WaitingToReconnect;
    case Source::Exhausted:
        return Target::Exhausted;
    }
    return Target::Disconnected;
}
QString scope(const core::ClientReadRequest& value)
{
    // Новое чтение той же области отменяет применение более старого результата.
    return QString{"%1:%2"}.arg(static_cast<int>(value.kind)).arg(value.chatIdentifier);
}
void finishPlanner(ui::ClientModels& models, const std::optional<core::ClientAction>& action, bool success,
                   const QString& message = {}, const QString& effect = {})
{
    if (action)
    {
        if (const auto* planner = std::get_if<core::ClientPlannerAction>(&*action))
        {
            models.finishPlannerAction(*planner, success, message, effect);
        }
    }
}
} // namespace

DesktopClientController::DesktopClientController(infra::TcpClient& client, ui::ClientModels& models, QObject* parent) :
    QObject{parent}, m_client{client}, m_models{models}
{
    connect(&client, &infra::TcpClient::stateChanged, this, [this](infra::TcpClientState) { updateState(); });
    connect(&client, &infra::TcpClient::connectionAttempt, this,
            [this](int attempt)
            {
                m_attempt = attempt;
                m_delay = 0;
                updateState();
            });
    connect(&client, &infra::TcpClient::retryScheduled, this,
            [this](int attempt, int delay)
            {
                m_attempt = attempt;
                m_delay = delay;
                updateState();
            });
    connect(&client, &infra::TcpClient::sessionReset, this, [this]() { m_models.invalidateSession(); });
    connect(&client, &infra::TcpClient::connectionError, this,
            [this](const QString&, const QString& message) { m_models.setError(message); });
    connect(&client, &infra::TcpClient::ready, this,
            [this](infra::TcpClientServerInfo, const QJsonObject& snapshot)
            {
                try
                {
                    const auto decoded = infra::ClientProtocol::decode({}, snapshot);
                    if (!m_models.applyResult(decoded, m_models.sessionRevision(), m_models.viewRevision()))
                    {
                        return;
                    }
                    m_models.refresh();
                }
                catch (const std::exception&)
                {
                    m_models.setError("Сервер прислал неверные данные. Последние значения сохранены.");
                    m_client.disconnectFromServer();
                }
            });
    connect(&models, &ui::ClientModels::readRequested, this,
            [this](core::ClientReadRequest request, quint64 session, quint64 view) { read(request, session, view); });
    connect(&models, &ui::ClientModels::actionRequested, this,
            [this](const core::ClientAction& action) { act(action); });
    connect(&models, &ui::ClientModels::reconnectRequested, this, [this]() { m_client.reconnect(); });
    connect(&client, &infra::TcpClient::responseReceived, this, [this](const auto& value) { response(value); });
    connect(&client, &infra::TcpClient::serverRejected, this,
            [this](const infra::ProtocolFailure& failure)
            {
                const auto found = m_pending.find(failure.requestId);
                if (found == m_pending.end())
                {
                    return;
                }
                const auto action = found->second.action;
                m_pending.erase(found);
                m_models.setError(failure.message, failure.effect, failure.userMessageId.value_or(QString{}));
                finishPlanner(m_models, action, false, failure.message, failure.effect);
                if (action)
                {
                    queueRefresh();
                }
            });
    connect(&client, &infra::TcpClient::requestFailed, this,
            [this](const infra::TcpClientRequestFailure& failure)
            {
                const auto found = m_pending.find(failure.requestId);
                if (found == m_pending.end())
                {
                    return;
                }
                const auto action = found->second.action;
                m_pending.erase(found);
                m_models.setError(failure.message, failure.effect);
                finishPlanner(m_models, action, false, failure.message, failure.effect);
            });
    connect(&client, &infra::TcpClient::eventReceived, this, [this](const infra::ProtocolEvent&) { queueRefresh(); });
    updateState();
    // Конструктор связывается до запуска клиента; не включает сеть сам.
}

void DesktopClientController::updateState()
{
    m_models.setConnectionState(state(m_client.state()), m_attempt, m_delay);
}

void DesktopClientController::read(core::ClientReadRequest request, std::uint64_t session, std::uint64_t view)
{
    if (!m_models.online() || session != m_models.sessionRevision())
    {
        return;
    }
    try
    {
        const auto wire = infra::ClientProtocol::request(request);
        const auto key = scope(request);
        const auto sequence = ++m_sequences[key];
        const auto requestId = m_client.sendRequest(wire.method, wire.params);
        m_pending.emplace(requestId, Context{request, std::nullopt, session, view, sequence, key});
    }
    catch (const std::exception&)
    {
        m_models.setError("Не удалось подготовить чтение данных.");
    }
}

void DesktopClientController::act(const core::ClientAction& action)
{
    if (!m_models.online())
    {
        m_models.setError("Нет связи: действие не отправлено.");
        finishPlanner(m_models, action, false, "Нет связи: действие не отправлено.", "none");
        return;
    }
    try
    {
        const auto wire = infra::ClientProtocol::request(action);
        const auto requestId = m_client.sendRequest(wire.method, wire.params);
        m_pending.emplace(requestId,
                          Context{std::nullopt, action, m_models.sessionRevision(), m_models.viewRevision(), 0, {}});
    }
    catch (const std::exception&)
    {
        m_models.setError("Действие содержит неверные данные.");
        finishPlanner(m_models, action, false, "Действие содержит неверные данные.", "none");
    }
}

void DesktopClientController::response(const infra::ProtocolSuccess& response)
{
    const auto found = m_pending.find(response.requestId);
    if (found == m_pending.end())
    {
        return;
    }
    const auto context = found->second;
    m_pending.erase(found);
    if (context.session != m_models.sessionRevision())
    {
        return;
    }
    try
    {
        if (context.read)
        {
            if (m_sequences[context.key] != context.sequence)
            {
                return;
            }
            const auto result = infra::ClientProtocol::decode(*context.read, response.result);
            static_cast<void>(m_models.applyResult(result, context.session, context.view));
        }
        else
        {
            infra::ClientProtocol::validateActionResult(*context.action, response.result);
            finishPlanner(m_models, context.action, true);
            if (const auto* retry = std::get_if<core::ClientRetryMessage>(&*context.action);
                retry && m_models.errorEffect() == "user_saved" &&
                m_models.savedUserIdentifier() == QString::number(retry->userMessageIdentifier))
            {
                m_models.clearError();
            }
            queueRefresh();
        }
    }
    catch (const std::exception&)
    {
        m_models.setError("Сервер прислал неверный результат. Последние значения сохранены.",
                          context.action ? "unknown" : "none");
        finishPlanner(m_models, context.action, false, "Сервер прислал неверный результат.", "unknown");
        m_client.disconnectFromServer();
    }
}

void DesktopClientController::queueRefresh()
{
    if (m_refreshQueued)
    {
        return;
    }
    m_refreshQueued = true;
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           m_refreshQueued = false;
                           m_models.refresh();
                       });
}
} // namespace msa::desktop
