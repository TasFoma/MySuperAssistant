#pragma once

#include "msa/core/Clock.h"
#include "msa/infra/ProtocolCodec.h"
#include "msa/infra/ProtocolMessage.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <map>
#include <set>

namespace msa::infra
{
/// TCP-транспорт версии 1. Все методы вызываются в потоке владельца с циклом событий.
/// Обработчик requestReceived должен быстро передать работу исполнителю и вернуть управление.
class TcpServer final : public QObject
{
    Q_OBJECT

public:
    /// Токен и IANA-зона задаются приложением; часы содержат местное время сервера.
    TcpServer(QString token, QString timeZone, const core::Clock& clock, QObject* parent = nullptr);
    /// Закрывает слушатель и соединения без отмены уже переданной предметной работы.
    ~TcpServer() override;
    /// Начать прослушивание явного адреса/порта; порт 0 позволяет ОС выбрать свободный.
    [[nodiscard]] bool listen(const QHostAddress& address, quint16 port);
    /// Остановить приём и закрыть соединения; идентификаторы повторно не используются.
    void close();
    /// Фактический порт слушателя, в том числе выбранный ОС.
    [[nodiscard]] quint16 serverPort() const;
    /// Идентификатор экземпляра сохраняется при повторном listen этого объекта.
    [[nodiscard]] QString serverInstanceId() const;
    /// Завершить ожидающий запрос; false означает устаревшую сессию/ID или неверный конверт.
    /// true означает постановку ответа в сокет, а не подтверждённую доставку клиенту.
    [[nodiscard]] bool sendResponse(quint64 connectionId, const ProtocolSuccess& response);
    /// Завершить запрос отказом, сохраняя указанные обработчиком последствия записи.
    [[nodiscard]] bool sendResponse(quint64 connectionId, const ProtocolFailure& response);
    /// Отправить проверенное событие всем авторизованным клиентам после commit.
    /// false означает неверное/слишком большое событие; воспроизведения после обрыва нет.
    [[nodiscard]] bool publishEvent(const QString& name, const QJsonObject& data);

signals:
    /// Проверенный запрос; connectionId и requestId вместе адресуют единственное завершение.
    void requestReceived(quint64 connectionId, msa::infra::ProtocolRequest request);
    /// Сессия закрыта; её временные предложения можно освободить. Записи не откатываются.
    void connectionClosed(quint64 connectionId);

private slots:
    void onNewConnection();

private:
    struct Session
    {
        QTcpSocket* socket{};
        QTimer* authTimer{};
        ProtocolFrameDecoder decoder{};
        bool authenticated{};
        bool closing{};
        std::set<QString> usedRequestIds{};
        std::set<QString> pendingRequestIds{};
    };

    void readConnection(quint64 connectionId);
    void processFrame(quint64 connectionId, const QByteArray& frame);
    void failProtocol(quint64 connectionId);
    void closeConnection(quint64 connectionId);
    void forgetConnection(quint64 connectionId);
    [[nodiscard]] bool writeMessage(quint64 connectionId, const ProtocolMessage& message);
    [[nodiscard]] bool completeRequest(quint64 connectionId, const QString& requestId, const ProtocolMessage& response);
    [[nodiscard]] bool rememberRequest(quint64 connectionId, const QString& requestId);

private:
    QString m_token;
    QString m_timeZone;
    const core::Clock& m_clock;
    QString m_serverInstanceId;
    QTcpServer m_listener{};
    std::map<quint64, Session> m_sessions{};
    quint64 m_nextConnectionId{1};
};
} // namespace msa::infra

Q_DECLARE_METATYPE(msa::infra::ProtocolRequest)
