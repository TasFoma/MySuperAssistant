#include "msa/infra/HttpGetClient.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedValueRollback>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace msa::infra
{
namespace
{

constexpr int kHttpOk = 200;

[[nodiscard]] core::ExternalServiceError unavailable(const std::string& reason)
{
    return {core::ExternalServiceErrorKind::Unavailable, reason};
}

} // namespace

HttpGetClient::HttpGetClient(QNetworkAccessManager& manager, HttpGetSettings settings) :
    m_manager{manager}, m_settings{std::move(settings)}
{
    if (m_settings.timeout.count() <= 0 || m_settings.timeout.count() > std::numeric_limits<int>::max() ||
        m_settings.userAgent.trimmed().isEmpty() || m_settings.userAgent.contains('\r') ||
        m_settings.userAgent.contains('\n'))
    {
        throw std::invalid_argument{"Укажите положительный срок запроса и User-Agent приложения."};
    }
}

bool HttpGetClient::validEndpoint(const QUrl& url)
{
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https") && !url.host().isEmpty() &&
           url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment();
}

core::ExternalServiceResult<QByteArray> HttpGetClient::get(const QUrl& url, std::chrono::milliseconds interval)
{
    if (!QCoreApplication::instance() || QThread::currentThread() != m_manager.thread() || m_inProgress)
    {
        return unavailable("Внешний сервис недоступен в текущем потоке или уже занят.");
    }
    QScopedValueRollback<bool> progress{m_inProgress, true};
    // Ожидание монотонное, повторный вход блокируется и во время ограничения частоты.
    while (std::chrono::steady_clock::now() < m_nextRequest)
    {
        const auto remaining =
            std::max(std::chrono::milliseconds{1},
                     std::chrono::ceil<std::chrono::milliseconds>(m_nextRequest - std::chrono::steady_clock::now()));
        QEventLoop waiting;
        QTimer timer;
        timer.setSingleShot(true);
        timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer, &QTimer::timeout, &waiting, &QEventLoop::quit);
        timer.start(static_cast<int>(remaining.count()));
        waiting.exec();
    }
    QNetworkRequest request{url};
    request.setRawHeader("User-Agent", m_settings.userAgent);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setTimerType(Qt::PreciseTimer);
    const std::unique_ptr<QNetworkReply> reply{m_manager.get(request)};
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(static_cast<int>(m_settings.timeout.count()));
    if (!reply->isFinished())
    {
        loop.exec();
    }
    // Пауза после завершения/тайм-аута гарантирует частоту и при задержке отправки Qt.
    m_nextRequest = std::chrono::steady_clock::now() + interval;
    if (!reply->isFinished())
    {
        reply->abort();
        return unavailable("Истекло время ожидания внешнего сервиса. Повторите запрос.");
    }
    if (reply->error() != QNetworkReply::NoError ||
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != kHttpOk)
    {
        return unavailable("Внешний сервис недоступен или отклонил запрос. Повторите позже.");
    }

    return reply->readAll();
}

} // namespace msa::infra
