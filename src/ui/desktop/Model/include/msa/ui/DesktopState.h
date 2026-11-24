#pragma once

#include "msa/core/ClientPreferences.h"
#include "msa/ui/AudioDevicesModel.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/ClientSettingsModel.h"
#include "msa/ui/MusicModel.h"
#include "msa/ui/PlannerEditor.h"
#include "msa/ui/SportLibraryModel.h"
#include "msa/ui/VoiceModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace msa::ui
{
/// @brief Навигация и форма настроек ПК, без файлов и транспорта.
class DesktopState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(msa::ui::SportLibraryModel* sport READ sport CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application runtime")
    Q_PROPERTY(msa::ui::AudioDevicesModel* audioDevices READ audioDevices CONSTANT)
    Q_PROPERTY(Page page READ page NOTIFY pageChanged)
    Q_PROPERTY(QStringList pages READ pages CONSTANT)
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(PlanSection planSection READ planSection NOTIFY planSectionChanged)
    Q_PROPERTY(QVariantList planSections READ planSections CONSTANT)
    Q_PROPERTY(QString todayDate READ todayDate NOTIFY todayDateChanged)
    Q_PROPERTY(QString host READ host NOTIFY preferencesChanged)
    Q_PROPERTY(QString port READ port NOTIFY preferencesChanged)
    Q_PROPERTY(QString token READ token NOTIFY preferencesChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect NOTIFY preferencesChanged)
    Q_PROPERTY(bool canConnect READ canConnect NOTIFY preferencesChanged)
    Q_PROPERTY(QString settingsMessage READ settingsMessage NOTIFY settingsMessageChanged)
    Q_PROPERTY(msa::ui::PlannerEditor* planner READ planner CONSTANT)
    Q_PROPERTY(msa::ui::MusicModel* music READ music CONSTANT)
    Q_PROPERTY(msa::ui::VoiceModel* voice READ voice CONSTANT)
public:
    /// @brief Офлайн-карточки и локальные предпочтения спорта.
    [[nodiscard]] SportLibraryModel* sport();
    /// @brief Разделы Desktop; значения не являются индексами экранов.
    enum class Page
    {
        Today,
        Plans,
        Workouts,
        Music,
        Assistant,
        Settings
    };
    Q_ENUM(Page)
    /// @brief Независимый выбор вкладки планировщика.
    enum class PlanSection
    {
        Tasks,
        Calendar,
        Habits,
        Reminders
    };
    Q_ENUM(PlanSection)
    /// @brief Модель выбора локального ввода/вывода, доступная без сервера.
    [[nodiscard]] AudioDevicesModel* audioDevices();
    /// @brief Принимает общие модели, живущие дольше состояния экрана.
    explicit DesktopState(ClientModels& models, QObject* parent = nullptr);
    /// @brief Единая форма действий планировщика.
    [[nodiscard]] PlannerEditor* planner();
    /// @brief Локальное управление музыкой во всех разделах, включая режим без сервера.
    [[nodiscard]] MusicModel* music();
    /// @brief Локальный голос и черновик, независимые от подключения к серверу.
    [[nodiscard]] VoiceModel* voice();
    /// @brief Выбранный раздел и подписи меню.
    [[nodiscard]] Page page() const;
    [[nodiscard]] QStringList pages() const;
    /// @brief Описания пунктов меню и вкладок для отображения в QML.
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QVariantList planSections() const;
    [[nodiscard]] PlanSection planSection() const;
    /// @brief Местная дата устройства, обновляемая при смене дня без сервера.
    [[nodiscard]] QString todayDate() const;
    /// @brief Последние успешно загруженные или сохранённые настройки.
    [[nodiscard]] QString host() const;
    [[nodiscard]] QString port() const;
    [[nodiscard]] QString token() const;
    [[nodiscard]] bool autoConnect() const;
    /// @brief Последние успешно сохранённые параметры для явного подключения в apps.
    [[nodiscard]] core::ClientPreferences preferences() const;
    /// @brief Есть корректный сохранённый набор; черновик QML сам соединение не меняет.
    [[nodiscard]] bool canConnect() const;
    /// @brief Запрашивает подключение по сохранённым параметрам без автоматического сохранения формы.
    Q_INVOKABLE void connectToServer();
    /// @brief Результат проверки или сохранения без секретных значений.
    [[nodiscard]] QString settingsMessage() const;
    /// @brief Применяет успешно прочитанный файл без подключения к сети.
    void applyPreferences(const core::ClientPreferences& preferences);
    /// @brief Сообщает итог файловой операции из связки приложения.
    void setSettingsMessage(const QString& message);
    /// @brief Локальный переход, разрешённый без сервера.
    Q_INVOKABLE void navigate(Page page);
    /// @brief Переключает представление планов без предметных запросов.
    Q_INVOKABLE void selectPlanSection(PlanSection section);
    /// @brief Проверяет форму и передаёт запрос сохранения связке приложения.
    Q_INVOKABLE bool savePreferences(const QString& host, const QString& port, const QString& token, bool autoConnect);
    /// @brief Запрашивает календарный день в формате ISO; правила остаются в C++.
    Q_INVOKABLE bool showCalendarDay(const QString& date);
signals:
    void pageChanged();                                           ///< @brief Изменён раздел.
    void planSectionChanged();                                    ///< @brief Изменена вкладка планов.
    void todayDateChanged();                                      ///< @brief Изменилась местная дата.
    void preferencesChanged();                                    ///< @brief Изменены сохранённые параметры.
    void settingsMessageChanged();                                ///< @brief Изменён результат операции с настройками.
    void saveRequested(msa::core::ClientPreferences preferences); ///< @brief Проверенные параметры для записи.
    void connectRequested();                                      ///< @brief Явное применение сохранённых параметров.
private:
    SportLibraryModel m_sport{this};
    ClientModels& m_models;
    PlannerEditor m_planner{m_models, this};
    MusicModel m_music{this};
    VoiceModel m_voice{this};
    AudioDevicesModel m_audioDevices{this};
    Page m_page{Page::Today};
    PlanSection m_planSection{PlanSection::Tasks};
    ClientSettingsModel m_settings{this};
    QTimer m_dateTimer{this};
    QString m_todayDate{};
};
} // namespace msa::ui
