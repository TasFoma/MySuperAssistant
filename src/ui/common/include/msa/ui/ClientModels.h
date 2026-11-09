#pragma once

#include "msa/core/ClientData.h"
#include "msa/ui/ClientListModel.h"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace msa::ui
{
/// @brief Общие модели представления; принимают только типы core и не знают о сети/JSON.
class ClientModels final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(msa::ui::ClientListModel* tasks READ tasks CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* reminders READ reminders CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* habits READ habits CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* eventsToday READ eventsToday CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* calendarEvents READ calendarEvents CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* chats READ chats CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* messages READ messages CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* profile READ profile CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* moods READ moods CONSTANT)
    Q_PROPERTY(msa::ui::ClientListModel* routes READ routes CONSTANT)
    Q_PROPERTY(bool online READ online NOTIFY connectionChanged)
    Q_PROPERTY(bool serverActionsEnabled READ online NOTIFY connectionChanged)
    Q_PROPERTY(bool reconnectAvailable READ reconnectAvailable NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY connectionChanged)
    Q_PROPERTY(QString mainChatIdentifier READ mainChatIdentifier NOTIFY metadataChanged)
    Q_PROPERTY(QString selectedChatIdentifier READ selectedChatIdentifier NOTIFY selectionChanged)
    Q_PROPERTY(QString timeZone READ timeZone NOTIFY metadataChanged)
    Q_PROPERTY(QString serverTime READ serverTime NOTIFY metadataChanged)
    Q_PROPERTY(bool messageHasMore READ messageHasMore NOTIFY paginationChanged)
    Q_PROPERTY(QVariantList monthDays READ monthDays NOTIFY calendarChanged)
    Q_PROPERTY(QVariantMap routeStatistics READ routeStatistics NOTIFY routeStatisticsChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(QString errorEffect READ errorEffect NOTIFY errorChanged)
    Q_PROPERTY(QString savedUserIdentifier READ savedUserIdentifier NOTIFY errorChanged)
public:
    /// @brief Создаёт пустые модели без действий во внешнем мире.
    explicit ClientModels(QObject* parent = nullptr);
    /// @brief Список tasks со стабильным временем жизни.
    [[nodiscard]] ClientListModel* tasks();
    /// @brief Список reminders со стабильным временем жизни.
    [[nodiscard]] ClientListModel* reminders();
    /// @brief Список habits со стабильным временем жизни.
    [[nodiscard]] ClientListModel* habits();
    /// @brief Список eventsToday со стабильным временем жизни.
    [[nodiscard]] ClientListModel* eventsToday();
    /// @brief Список calendarEvents со стабильным временем жизни.
    [[nodiscard]] ClientListModel* calendarEvents();
    /// @brief Список chats со стабильным временем жизни.
    [[nodiscard]] ClientListModel* chats();
    /// @brief Список messages со стабильным временем жизни.
    [[nodiscard]] ClientListModel* messages();
    /// @brief Список profile со стабильным временем жизни.
    [[nodiscard]] ClientListModel* profile();
    /// @brief Список moods со стабильным временем жизни.
    [[nodiscard]] ClientListModel* moods();
    /// @brief Список routes со стабильным временем жизни.
    [[nodiscard]] ClientListModel* routes();
    /// @brief Готовы транспорт и проверенный снимок текущей сессии.
    [[nodiscard]] bool online() const;
    /// @brief Кнопка ручного подключения доступна после отключения или исчерпания пяти попыток.
    [[nodiscard]] bool reconnectAvailable() const;
    /// @brief Машинное имя состояния для QML.
    [[nodiscard]] QString connectionState() const;
    /// @brief Русский текст состояния, попытки и задержки.
    [[nodiscard]] QString statusText() const;
    /// @brief Служебная беседа не является выбранным экраном.
    [[nodiscard]] QString mainChatIdentifier() const;
    /// @brief Локально выбранная беседа либо пустая строка.
    [[nodiscard]] QString selectedChatIdentifier() const;
    /// @brief Зона и местный момент последнего принятого снимка.
    [[nodiscard]] QString timeZone() const;
    [[nodiscard]] QString serverTime() const;
    /// @brief Для текущего чата ещё доступны страницы.
    [[nodiscard]] bool messageHasMore() const;
    /// @brief Дни выбранного месяца, содержащие события.
    [[nodiscard]] QVariantList monthDays() const;
    /// @brief Статистика прогнозов маршрутов, полученная с сервера без пересчёта в UI.
    [[nodiscard]] QVariantMap routeStatistics() const;
    /// @brief Ошибка и последствия последней операции; unknown не является разрешением повтора.
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString errorEffect() const;
    [[nodiscard]] QString savedUserIdentifier() const;
    /// @brief Метки сессии/выбора для отбрасывания устаревших ответов.
    [[nodiscard]] std::uint64_t sessionRevision() const;
    [[nodiscard]] std::uint64_t viewRevision() const;
    /// @brief Переносит состояние подключения, не очищая последние строки.
    void setConnectionState(core::ClientConnectionState state, int attempt = 0, int retryDelayMs = 0);
    /// @brief Инвалидирует ответы сессии и отмечает все списки устаревшими.
    void invalidateSession();
    /// @brief Применяет проверенный результат только подходящему контексту сессии/экрана.
    [[nodiscard]] bool applyResult(const core::ClientReadResult& result, std::uint64_t session, std::uint64_t view);
    /// @brief Устанавливает понятную ошибку без технических данных протокола.
    void setError(const QString& message, const QString& effect = "none", const QString& savedUser = {});
    /// @brief Локально выбирает существующий чат, включая просмотр кэша без связи.
    Q_INVOKABLE bool selectChat(const QString& identifier);
    /// @brief Запрашивает следующую страницу только при готовом соединении.
    Q_INVOKABLE bool loadMoreMessages();
    /// @brief Перечитывает данные; записывающие действия не повторяет.
    Q_INVOKABLE void refresh();
    /// @brief Запрашивает выбранные день/месяц календаря.
    Q_INVOKABLE bool showDay(const QString& date);
    Q_INVOKABLE bool showMonth(int year, int month);
    /// @brief Отправляет типизированные действия только при готовом соединении.
    Q_INVOKABLE bool setTaskDone(const QString& identifier, bool done);
    Q_INVOKABLE bool sendMessage(const QString& text, const QString& searchQuery = {});
    Q_INVOKABLE bool retryMessage(const QString& userIdentifier, const QString& searchQuery = {});
    Q_INVOKABLE bool createChat();
    /// @brief Ручное переподключение передаётся связке приложения.
    Q_INVOKABLE void reconnect();
    /// @brief Убирает сообщение ошибки по явному действию пользователя.
    Q_INVOKABLE void clearError();
signals:
    void connectionChanged();      ///< @brief Изменились состояние/доступность действий.
    void metadataChanged();        ///< @brief Приняты метаданные снимка.
    void selectionChanged();       ///< @brief Локально выбран другой чат.
    void paginationChanged();      ///< @brief Изменился курсор/признак страниц.
    void calendarChanged();        ///< @brief Изменились дни выбранного месяца.
    void routeStatisticsChanged(); ///< @brief Обновилась статистика расчётов.
    void errorChanged();           ///< @brief Изменилось сообщение об операции.
    /// @brief Чтение выполняет apps, переданные ревизии возвращаются вместе с результатом.
    void readRequested(msa::core::ClientReadRequest request, quint64 session, quint64 view);
    /// @brief Явное действие; в отключённом состоянии не испускается.
    void actionRequested(msa::core::ClientAction action);
    void reconnectRequested(); ///< @brief Пользователь нажал переподключение.
private:
    struct ChatCache
    {
        std::vector<core::ChatMessage> messages{};
        std::optional<std::int64_t> cursor{};
        bool hasMore{true};
        std::uint64_t session{};
    };
    void requestRead(core::ClientReadRequest request);
    void refreshMessages(bool restart);
    [[nodiscard]] bool submit(core::ClientAction action);
    [[nodiscard]] bool applyPage(const core::ClientMessagePage& page, std::uint64_t view);
    void displayMessages();

private:
    ClientListModel m_tasks{ClientListKind::Tasks, this};
    ClientListModel m_reminders{ClientListKind::Reminders, this};
    ClientListModel m_habits{ClientListKind::Habits, this};
    ClientListModel m_eventsToday{ClientListKind::Events, this};
    ClientListModel m_calendarEvents{ClientListKind::Events, this};
    ClientListModel m_chats{ClientListKind::Chats, this};
    ClientListModel m_messages{ClientListKind::Messages, this};
    ClientListModel m_profile{ClientListKind::Profile, this};
    ClientListModel m_moods{ClientListKind::Moods, this};
    ClientListModel m_routes{ClientListKind::Routes, this};
    core::ClientConnectionState m_state{core::ClientConnectionState::Disconnected};
    bool m_hasSnapshot{false};
    int m_attempt{0};
    int m_retryDelayMs{0};
    std::uint64_t m_sessionRevision{1};
    std::uint64_t m_viewRevision{0};
    std::int64_t m_mainChat{};
    std::int64_t m_selectedChat{};
    std::string m_timeZone{};
    std::chrono::local_seconds m_serverTime{};
    std::map<std::int64_t, ChatCache> m_history{};
    std::optional<std::chrono::year_month_day> m_day{};
    std::optional<std::chrono::year_month> m_month{};
    QVariantList m_monthDays{};
    QVariantMap m_routeStatistics{};
    QString m_error{};
    QString m_effect{"none"};
    QString m_savedUser{};
};
} // namespace msa::ui

Q_DECLARE_METATYPE(msa::core::ClientReadRequest)
Q_DECLARE_METATYPE(msa::core::ClientAction)
