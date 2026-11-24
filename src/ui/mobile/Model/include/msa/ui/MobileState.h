#pragma once

#include "msa/ui/ClientModels.h"
#include "msa/ui/ClientSettingsModel.h"
#include "msa/ui/PlannerEditor.h"
#include "msa/ui/SportLibraryModel.h"

#include <QtQml/qqmlregistration.h>

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

namespace msa::ui
{
/// @brief Навигация телефона, черновик и календарная сетка; без сети и файлов.
class MobileState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(msa::ui::SportLibraryModel* sport READ sport CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application runtime")
    Q_PROPERTY(Page page READ page NOTIFY navigationChanged)
    Q_PROPERTY(QStringList pages READ pages CONSTANT)
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(QVariantList planSections READ planSections CONSTANT)
    Q_PROPERTY(PlanSection planSection READ planSection NOTIFY navigationChanged)
    Q_PROPERTY(MoreSection moreSection READ moreSection NOTIFY navigationChanged)
    Q_PROPERTY(QString todayDate READ todayDate NOTIFY todayDateChanged)
    Q_PROPERTY(bool conversationsVisible READ conversationsVisible NOTIFY navigationChanged)
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY draftChanged)
    Q_PROPERTY(QString monthTitle READ monthTitle NOTIFY calendarChanged)
    Q_PROPERTY(QString selectedDate READ selectedDate NOTIFY calendarChanged)
    Q_PROPERTY(QVariantList calendarCells READ calendarCells NOTIFY calendarChanged)
    Q_PROPERTY(msa::ui::PlannerEditor* planner READ planner CONSTANT)
    Q_PROPERTY(msa::ui::ClientSettingsModel* settings READ settings CONSTANT)
public:
    /// @brief Офлайн-карточки и локальные предпочтения спорта.
    [[nodiscard]] SportLibraryModel* sport();
    /// @brief Основные вкладки, независимые от порядка экранов.
    enum class Page
    {
        Today,
        Plans,
        Sport,
        More
    };
    Q_ENUM(Page)
    /// @brief Вложенные экраны раздела «Ещё».
    enum class MoreSection
    {
        Menu,
        Assistant,
        Settings
    };
    Q_ENUM(MoreSection)
    /// @brief Вкладки существующего планировщика.
    enum class PlanSection
    {
        Tasks,
        Calendar,
        Habits,
        Reminders
    };
    Q_ENUM(PlanSection)
    /// @brief Общие модели должны жить дольше состояния экранов.
    explicit MobileState(ClientModels& models, QObject* parent = nullptr);
    [[nodiscard]] Page page() const;
    [[nodiscard]] QStringList pages() const;
    /// @brief Подписи и иконки вкладок для QML.
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QVariantList planSections() const;
    [[nodiscard]] PlanSection planSection() const;
    [[nodiscard]] MoreSection moreSection() const;
    /// @brief Местная дата устройства; календарь по-прежнему использует дату сервера.
    [[nodiscard]] QString todayDate() const;
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
    Q_INVOKABLE void navigate(Page page);
    /// @brief Переходы внутри планов и раздела «Ещё» без сетевых действий.
    Q_INVOKABLE void selectPlanSection(PlanSection section);
    Q_INVOKABLE void showMore(MoreSection section);
    Q_INVOKABLE void showConversations();
    Q_INVOKABLE bool selectConversation(const QString& identifier);
    /// @brief Возвращается по мобильной иерархии; изменённая форма требует подтверждения отмены.
    Q_INVOKABLE bool goBack();
    /// @brief Явная отправка и повтор с сохранением черновика и выбора поисковой операции.
    Q_INVOKABLE bool sendMessage();
    Q_INVOKABLE bool retryMessage();
    /// @brief Сдвигает календарь на один месяц, проверяя диапазон 1..9999.
    Q_INVOKABLE void shiftMonth(int offset);
    Q_INVOKABLE bool selectDay(const QString& date);
signals:
    void todayDateChanged();  ///< @brief Изменилась местная дата устройства.
    void navigationChanged(); ///< @brief Изменён локальный экран.
    void draftChanged();      ///< @brief Изменён ввод без отправки.
    void calendarChanged();   ///< @brief Изменена сетка или её отметки.
private:
    void updateServerDate();
    void refreshCalendar();

private:
    SportLibraryModel m_sport{this};
    ClientModels& m_models;
    PlannerEditor m_planner{m_models, this};
    ClientSettingsModel m_settings{this};
    Page m_page{Page::Today};
    PlanSection m_planSection{PlanSection::Tasks};
    MoreSection m_moreSection{MoreSection::Menu};
    QTimer m_dateTimer{this};
    QString m_todayDate{};
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
