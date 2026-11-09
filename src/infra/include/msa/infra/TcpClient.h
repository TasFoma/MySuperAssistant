#pragma once

#include "msa/infra/ProtocolCodec.h"
#include "msa/infra/ProtocolMessage.h"

#include <QJsonObject>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include <chrono>
#include <map>
#include <optional>
#include <set>

namespace msa::infra {

/// @brief Состояние транспорта; локальные функции приложения не зависят от соединения.
enum class TcpClientState {
    Disconnected, Connecting, Authenticating, Synchronizing, Ready, WaitingToReconnect, Exhausted,
};

/// @brief Явный endpoint и сроки; задержки повторов не содержат личных значений.
struct TcpClientSettings {
    /// @brief Имя хоста или адрес, без схемы URL.
    QString host{};
    /// @brief Ненулевой порт сервера.
    quint16 port{};
    /// @brief Токен auth, никогда не возвращается в ошибках.
    QString token{};
    /// @brief Положительный общий срок подключения и отдельной авторизации.
    std::chrono::milliseconds connectTimeout{};
    /// @brief Положительный срок get_all и каждого предметного запроса.
    std::chrono::milliseconds responseTimeout{};
    /// @brief Первая задержка автоматического повтора.
    std::chrono::milliseconds initialReconnectDelay{1000};
    /// @brief Верхний предел возрастающей задержки.
    std::chrono::milliseconds maximumReconnectDelay{15000};
};

/// @brief Проверенные метаданные auth, принадлежащие текущей TCP-сессии.
struct TcpClientServerInfo {
    /// @brief ID процесса сервера, меняется после перезапуска.
    QString instanceId{};
    /// @brief Местное время сервера без выдуманного UTC-суффикса.
    QString serverTime{};
    /// @brief IANA-зона серверного времени.
    QString timeZone{};
    /// @brief Поддержанный предел кадра без LF.
    int maxFrameBytes{};
};

/// @brief Локальный отказ; unknown запрещает автоматически повторять отправленную запись.
struct TcpClientRequestFailure {
    QString requestId{}; ///< @brief ID единственного запроса, не нового соединения.
    QString method{}; ///< @brief Имя исходной операции.
    QString code{}; ///< @brief Локальная категория отказа, без технических секретов.
    QString message{}; ///< @brief Понятная причина.
    QString effect{}; ///< @brief none до отправки/для чтения, unknown для ожидавшей записи.
};

/// @brief Асинхронный транспорт версии 1; методы вызываются в потоке создания с циклом Qt.
/// Приложение владеет объектом, UI и постоянные настройки здесь не создаются.
class TcpClient final : public QObject {
    Q_OBJECT

public:
    /// @brief Пять попыток на цикл; Ready и ручное подключение сбрасывают счёт.
    static constexpr int kMaximumConnectionAttempts{5};
    /// @brief Проверяет настройки, создаёт отключённый клиент без сетевых действий.
    explicit TcpClient(TcpClientSettings settings, QObject* parent = nullptr);
    /// @brief Отменяет таймеры и закрывает сокет без внешних уведомлений из деструктора.
    ~TcpClient() override;
    /// @brief Начинает новый цикл из Disconnected/Exhausted; активный цикл не дублирует.
    void connectToServer();
    /// @brief Прерывает прежнюю сессию и начинает новый цикл по явному действию пользователя.
    void reconnect();
    /// @brief Отключает транспорт и отменяет автоматические попытки, сохраняя приложение работающим.
    void disconnectFromServer();
    /// @brief Отправляет запрос только в Ready, возвращает новый ID до уведомления о локальном отказе.
    /// Неверные/слишком большие запросы и вызовы вне Ready не ставятся в очередь на отправку.
    [[nodiscard]] QString sendRequest(const QString& method, const QJsonObject& params = {});
    /// @brief Текущее состояние без обращения к сети.
    [[nodiscard]] TcpClientState state() const;
    /// @brief Число попыток текущего цикла; в Ready равно нулю.
    [[nodiscard]] int connectionAttempts() const;
    /// @brief Метаданные текущей авторизованной сессии; после обрыва отсутствуют.
    [[nodiscard]] std::optional<TcpClientServerInfo> serverInfo() const;
    /// @brief Начальный снимок Ready; после обрыва пуст. Постоянный кэш принадлежит приложению.
    [[nodiscard]] QJsonObject initialSnapshot() const;

signals:
    /// @brief Состояние изменилось; Exhausted позволяет показать кнопку переподключения.
    void stateChanged(msa::infra::TcpClientState state);
    /// @brief Начата фактическая попытка с номером от одного до пяти.
    void connectionAttempt(int attempt);
    /// @brief Запланирована ограниченная задержка следующей попытки.
    void retryScheduled(int nextAttempt, int delayMs);
    /// @brief Лимит попыток исчерпан, новых таймеров подключения нет.
    void retryExhausted();
    /// @brief Временные данные/предложения прежней сессии недействительны.
    void sessionReset();
    /// @brief Авторизация и начальный get_all завершены; экран беседы не выбирается.
    void ready(msa::infra::TcpClientServerInfo info, QJsonObject snapshot);
    /// @brief Ошибка подключения/согласования без адреса, токена и технических тел.
    void connectionError(QString code, QString message);
    /// @brief Один коррелированный предметный успех; схему result проверяет адаптер метода.
    void responseReceived(msa::infra::ProtocolSuccess response);
    /// @brief Один коррелированный предметный отказ сервера с исходным effect.
    void serverRejected(msa::infra::ProtocolFailure failure);
    /// @brief Локальный отказ/обрыв ожидающего запроса; ID никогда не переиспользуется клиентом.
    void requestFailed(msa::infra::TcpClientRequestFailure failure);
    /// @brief Проверенный конверт события текущего экземпляра; возможен уже в Synchronizing.
    void eventReceived(msa::infra::ProtocolEvent event);

private slots:
    void onRetry();
    void onPhaseTimeout();
    void onRequestTimeout();

private:
    struct PendingRequest {
        QString method{};
        bool readOnly{};
        std::chrono::steady_clock::time_point deadline{};
    };

    void beginAttempt();
    void onConnected(quint64 generation);
    void readSocket(quint64 generation);
    void processFrame(const QByteArray& frame, quint64 generation);
    void processSuccess(const ProtocolSuccess& response, quint64 generation);
    void processFailure(const ProtocolFailure& failure);
    void endSession(const QString& code, const QString& message, bool retry, bool report);
    void detachSocket();
    void setState(TcpClientState state);
    void scheduleRequestTimeout();
    void postFailure(TcpClientRequestFailure failure);
    [[nodiscard]] bool writeBytes(const QByteArray& bytes);
    [[nodiscard]] int retryDelay() const;
    [[nodiscard]] QString nextRequestId();

private:
    TcpClientSettings m_settings;
    TcpClientState m_state{TcpClientState::Disconnected};
    QPointer<QTcpSocket> m_socket{};
    QTimer m_retryTimer{};
    QTimer m_phaseTimer{};
    QTimer m_requestTimer{};
    ProtocolFrameDecoder m_decoder{};
    std::map<QString, PendingRequest> m_pending{};
    std::set<QString> m_seenEvents{};
    std::optional<TcpClientServerInfo> m_info{};
    QJsonObject m_snapshot{};
    QString m_authRequest{};
    QString m_syncRequest{};
    quint64 m_generation{0};
    quint64 m_nextRequest{1};
    int m_attempts{0};
    bool m_enabled{false};
};
} // namespace msa::infra

Q_DECLARE_METATYPE(msa::infra::TcpClientState)
Q_DECLARE_METATYPE(msa::infra::TcpClientServerInfo)
Q_DECLARE_METATYPE(msa::infra::TcpClientRequestFailure)

