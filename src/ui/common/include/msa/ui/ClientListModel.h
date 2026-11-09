#pragma once

#include "msa/core/ClientData.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>
#include <variant>
#include <vector>

namespace msa::ui
{
/// @brief Строка типизированного клиентского списка, без JSON и сетевого поведения.
using ClientListRow = std::variant<core::Task, core::TimedReminder, core::RecurringReminder, core::PlaceReminder,
                                   core::HabitProgress, core::ClientEventOccurrence, core::Chat, core::ChatMessage,
                                   core::UserProfileFact, core::MoodEntry, core::RouteCalculation>;
/// @brief Вид списка задаёт допустимые строки и не меняется после создания.
enum class ClientListKind
{
    Tasks,
    Reminders,
    Habits,
    Events,
    Chats,
    Messages,
    Profile,
    Moods,
    Routes
};

/// @brief Общая QAbstractListModel с именованными ролями и явной устарелостью данных.
class ClientListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
public:
    /// @brief Создаёт пустой список заданного вида.
    explicit ClientListModel(ClientListKind kind, QObject* parent = nullptr);
    /// @brief Число строк; дочерних элементов нет.
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    /// @brief Значение именованной роли, ID всегда представлен строкой.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// @brief Стабильные имена ролей для QML, отсутствующее значение — невалидный QVariant.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    /// @brief Число элементов для свойства count.
    [[nodiscard]] int count() const;
    /// @brief Данные требуют обновления, но остаются доступны для просмотра.
    [[nodiscard]] bool stale() const;
    /// @brief Меняет только признак свежести без удаления строк.
    void setStale(bool stale);
    /// @brief Проверяет виды строк и уникальные положительные ID до замены списка.
    [[nodiscard]] bool accepts(const std::vector<ClientListRow>& rows) const;
    /// @brief Полностью заменяет проверенные строки, при ошибке оставляет прежние.
    [[nodiscard]] bool replaceRows(const std::vector<ClientListRow>& rows, bool stale = false);
    /// @brief Наличие строкового ID, без потери точности int64.
    [[nodiscard]] bool containsIdentifier(const QString& identifier) const;
signals:
    void countChanged(); ///< @brief Изменилось число строк.
    void staleChanged(); ///< @brief Изменился признак свежести.
private:
    ClientListKind m_kind;
    std::vector<QVariantMap> m_rows{};
    bool m_stale{true};
};
} // namespace msa::ui
