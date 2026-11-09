#pragma once

#include "msa/core/ClientPreferences.h"
#include "msa/ui/ClientModels.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace msa::ui
{
/// @brief Навигация и форма настроек ПК, без файлов и транспорта.
class DesktopState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int page READ page NOTIFY pageChanged)
    Q_PROPERTY(QStringList pages READ pages CONSTANT)
    Q_PROPERTY(QString host READ host NOTIFY preferencesChanged)
    Q_PROPERTY(QString port READ port NOTIFY preferencesChanged)
    Q_PROPERTY(QString token READ token NOTIFY preferencesChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect NOTIFY preferencesChanged)
    Q_PROPERTY(QString settingsMessage READ settingsMessage NOTIFY settingsMessageChanged)
public:
    /// @brief Принимает общие модели, живущие дольше состояния экрана.
    explicit DesktopState(ClientModels& models, QObject* parent = nullptr);
    /// @brief Выбранный раздел и подписи меню.
    [[nodiscard]] int page() const;
    [[nodiscard]] QStringList pages() const;
    /// @brief Последние успешно загруженные или сохранённые настройки.
    [[nodiscard]] QString host() const;
    [[nodiscard]] QString port() const;
    [[nodiscard]] QString token() const;
    [[nodiscard]] bool autoConnect() const;
    /// @brief Результат проверки или сохранения без секретных значений.
    [[nodiscard]] QString settingsMessage() const;
    /// @brief Применяет успешно прочитанный файл без подключения к сети.
    void applyPreferences(const core::ClientPreferences& preferences);
    /// @brief Сообщает итог файловой операции из связки приложения.
    void setSettingsMessage(const QString& message);
    /// @brief Локальный переход, разрешённый без сервера.
    Q_INVOKABLE void navigate(int page);
    /// @brief Проверяет форму и передаёт запрос сохранения связке приложения.
    Q_INVOKABLE bool savePreferences(const QString& host, const QString& port, const QString& token, bool autoConnect);
    /// @brief Запрашивает календарный день в формате ISO; правила остаются в C++.
    Q_INVOKABLE bool showCalendarDay(const QString& date);
signals:
    void pageChanged();                                           ///< @brief Изменён раздел.
    void preferencesChanged();                                    ///< @brief Изменены сохранённые параметры.
    void settingsMessageChanged();                                ///< @brief Изменён результат операции с настройками.
    void saveRequested(msa::core::ClientPreferences preferences); ///< @brief Проверенные параметры для записи.
private:
    ClientModels& m_models;
    int m_page{0};
    core::ClientPreferences m_preferences{};
    QString m_settingsMessage{};
};
} // namespace msa::ui
