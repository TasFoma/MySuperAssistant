#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QTcpSocket>

#include <functional>
#include <vector>

namespace msa::infra::test {

/// @brief Обслуживает события до условия; предикат должен сохранять найденный результат.
[[nodiscard]] bool waitForTest(const std::function<bool()>& predicate);

/// @brief Независимый TCP/JSON-клиент тестов: не использует серверный кодек и не повторяет записи.
class ProtocolTestClient {
public:
    /// @brief Создаёт отключённый клиент без глобального состояния.
    ProtocolTestClient();
    /// @brief Открывает новую сессию и очищает её кадры; авторизация выполняется отдельно.
    void connectTo(quint16 port);
    /// @brief Отправляет auth версии 1 и возвращает коррелированный ответ.
    [[nodiscard]] QJsonObject authenticate(const QString& token = "process-test-token");
    /// @brief Отправляет запрос ровно один раз; пустой requestId получает новый номер.
    [[nodiscard]] QJsonObject request(const QString& method, const QJsonObject& params = {},
        const QString& requestId = {});
    /// @brief Передаёт запрос без ожидания, позволяя оборвать соединение до получения ответа.
    void sendRequest(const QString& requestId, const QString& method, const QJsonObject& params = {});
    /// @brief Ожидает ответ с ID, сохраняя перемежающиеся ответы и события в очереди.
    [[nodiscard]] QJsonObject response(const QString& requestId);
    /// @brief Передаёт сырые байты, включая части UTF-8 и кадр без LF.
    void sendBytes(const QByteArray& bytes);
    /// @brief Немедленно закрывает сессию; запроса повторной записи не выполняет.
    void disconnect();
    /// @brief Ожидает закрытия сервером, не превращая хвост в сообщение.
    [[nodiscard]] bool waitDisconnected();
    /// @brief Все целые кадры текущей сессии для проверки отсутствия чужого позднего ответа.
    [[nodiscard]] const std::vector<QJsonObject>& frames();

private:
    void collect();
    void sendObject(const QJsonObject& object);

    QTcpSocket m_socket{};
    std::vector<QJsonObject> m_pending{};
    std::vector<QJsonObject> m_frames{};
    int m_requestNumber{0};
};
} // namespace msa::infra::test

