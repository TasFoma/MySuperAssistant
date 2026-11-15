#pragma once

#include "msa/ui/ClientModels.h"
#include "msa/ui/ClientSettingsModel.h"
#include "msa/ui/PlannerEditor.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace msa::ui
{
/// @brief Навигация телефона, черновик и календарная сетка; без сети и файлов.
class MobileState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int page READ page NOTIFY navigationChanged)
    Q_PROPERTY(QStringList pages READ pages CONSTANT)
    Q_PROPERTY(bool conversationsVisible READ conversationsVisible NOTIFY navigationChanged)
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY draftChanged)
    Q_PROPERTY(QString monthTitle READ monthTitle NOTIFY calendarChanged)
    Q_PROPERTY(QString selectedDate READ selectedDate NOTIFY calendarChanged)
    Q_PROPERTY(QVariantList calendarCells READ calendarCells NOTIFY calendarChanged)
    Q_PROPERTY(msa::ui::PlannerEditor* planner READ planner CONSTANT)
    Q_PROPERTY(msa::ui::ClientSettingsModel* settings READ settings CONSTANT)
public:
    /// @brief Общие модели должны жить дольше состояния экранов.
    explicit MobileState(ClientModels& models, QObject* parent = nullptr);
    [[nodiscard]] int page() const;
    [[nodiscard]] QStringList pages() const;
    [[nodiscard]] bool conversationsVisible() const;
    [[nodiscard]] QString draft() const;
    [[nodiscard]] QString searchQuery() const;
    [[nodiscard]] QString monthTitle() const;
    [[nodiscard]] QString selectedDate() const;
    /// @brief Готовые ячейки с понедельника; отметки событий берутся только с сервера.
    [[nodiscard]] QVariantList calendarCells() const;
    [[nodiscard]] PlannerEditor* planner();
    [[nodiscard]] ClientSettingsModel* settings();
    /// @brief Черновик не отправляется при смене вкладки или беседы.
    void setDraft(const QString& text);
    void setSearchQuery(const QString& text);
    /// @brief Локальные переходы доступны без связи.
    Q_INVOKABLE void navigate(int page);
    Q_INVOKABLE void showConversations();
    Q_INVOKABLE bool selectConversation(const QString& identifier);
    /// @brief Возвращается из списка бесед или на вкладку чата; форму отменяет без записи.
    Q_INVOKABLE bool goBack();
    /// @brief Явная отправка и повтор с сохранением черновика и выбора поисковой операции.
    Q_INVOKABLE bool sendMessage();
    Q_INVOKABLE bool retryMessage();
    /// @brief Сдвигает календарь на один месяц, проверяя диапазон 1..9999.
    Q_INVOKABLE void shiftMonth(int offset);
    Q_INVOKABLE bool selectDay(const QString& date);
signals:
    void navigationChanged(); ///< @brief Изменён локальный экран.
    void draftChanged();      ///< @brief Изменён ввод без отправки.
    void calendarChanged();   ///< @brief Изменена сетка или её отметки.
private:
    void updateServerDate();
    void refreshCalendar();

private:
    ClientModels& m_models;
    PlannerEditor m_planner{m_models, this};
    ClientSettingsModel m_settings{this};
    int m_page{0};
    bool m_conversationsVisible{false};
    QString m_draft{};
    QString m_searchQuery{};
    QString m_retrySearchQuery{};
    QString m_retryChat{};
    QDate m_today{};
    QDate m_month{};
    QDate m_selectedDate{};
    bool m_monthLoaded{false};
};
} // namespace msa::ui
