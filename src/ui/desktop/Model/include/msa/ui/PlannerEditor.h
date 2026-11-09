#pragma once

#include "msa/ui/ClientModels.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace msa::ui
{
/// @brief Состояние одной формы планировщика, без JSON, файлов и транспорта.
class PlannerEditor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY formChanged)
    Q_PROPERTY(QString heading READ heading NOTIFY formChanged)
    Q_PROPERTY(QString explanation READ explanation NOTIFY formChanged)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY formChanged)
    Q_PROPERTY(QVariantMap fieldVisibility READ fieldVisibility NOTIFY statusChanged)
    Q_PROPERTY(bool destructive READ destructive NOTIFY formChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY statusChanged)
    Q_PROPERTY(bool editable READ editable NOTIFY statusChanged)
    Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
public:
    /// @brief Использует общие модели; они должны жить дольше формы.
    explicit PlannerEditor(ClientModels& models, QObject* parent = nullptr);
    [[nodiscard]] bool active() const;
    [[nodiscard]] QString heading() const;
    [[nodiscard]] QString explanation() const;
    [[nodiscard]] QVariantList fields() const;
    /// @brief Видимость полей без пересоздания делегатов и потери клавиатурного фокуса.
    [[nodiscard]] QVariantMap fieldVisibility() const;
    [[nodiscard]] bool destructive() const;
    [[nodiscard]] bool pending() const;
    [[nodiscard]] bool editable() const;
    [[nodiscard]] bool canSubmit() const;
    [[nodiscard]] QString errorText() const;
    /// @brief Открывают форму; изменение заполняется ролями конкретной записи.
    Q_INVOKABLE void newTask();
    Q_INVOKABLE void editTask(const QString& identifier);
    Q_INVOKABLE void deleteTask(const QString& identifier);
    Q_INVOKABLE void newEvent();
    Q_INVOKABLE void editEvent(const QString& identifier);
    Q_INVOKABLE void deleteEvent(const QString& identifier);
    Q_INVOKABLE void newHabit();
    Q_INVOKABLE void recordHabit(const QString& identifier);
    Q_INVOKABLE void deleteHabit(const QString& identifier);
    Q_INVOKABLE void newReminder();
    Q_INVOKABLE void deleteReminder(const QString& identifier);
    /// @brief Изменяет текстовое поле или выбранный вариант, не выполняя запись.
    Q_INVOKABLE void setField(const QString& key, const QString& value);
    Q_INVOKABLE void setChoice(const QString& key, int index);
    /// @brief Проверяет форму и запрашивает ровно одну запись; данные не меняет оптимистично.
    Q_INVOKABLE bool submit();
    /// @brief Закрывает форму без действия, если подтверждение не ожидается.
    Q_INVOKABLE void cancel();
signals:
    void formChanged();   ///< @brief Изменились вид формы или её поля.
    void statusChanged(); ///< @brief Изменились проверка, ожидание и доступность действий.
private:
    enum class FormKind
    {
        NewTask,
        EditTask,
        NewEvent,
        EditEvent,
        NewHabit,
        LogHabit,
        NewReminder,
        DeleteTask,
        DeleteEvent,
        DeleteHabit,
        DeleteReminder
    };
    struct Field
    {
        QString key;
        QString label;
        QStringList values{};
        QStringList labels{};
        bool multiline{false};
    };
    void open(FormKind kind, const QString& identifier = {});
    void addField(const QString& key, const QString& label, const QString& value = {}, const QStringList& values = {},
                  const QStringList& labels = {}, bool multiline = false);
    [[nodiscard]] QVariantMap source(FormKind kind, const QString& identifier) const;
    [[nodiscard]] core::ClientPlannerAction buildAction() const;
    [[nodiscard]] core::ClientPlannerAction taskAction() const;
    [[nodiscard]] core::ClientPlannerAction eventAction() const;
    [[nodiscard]] core::ClientPlannerAction reminderAction() const;
    [[nodiscard]] QString value(const QString& key) const;
    void setError(const QString& message);

private:
    ClientModels& m_models;
    FormKind m_kind{FormKind::NewTask};
    bool m_active{false};
    bool m_requiresReopen{false};
    std::uint64_t m_session{};
    QString m_identifier{};
    QString m_heading{};
    QString m_explanation{};
    QString m_error{};
    std::vector<Field> m_fields{};
    std::map<QString, QString> m_values{};
    std::optional<core::ClientPlannerAction> m_pending{};
};
} // namespace msa::ui
