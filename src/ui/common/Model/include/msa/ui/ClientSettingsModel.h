#pragma once

#include "msa/core/ClientPreferences.h"

#include <QObject>
#include <QString>

namespace msa::ui
{
/// @brief Общая форма подключения; файлы и сеть обслуживает связка приложения.
class ClientSettingsModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host NOTIFY preferencesChanged)
    Q_PROPERTY(QString port READ port NOTIFY preferencesChanged)
    Q_PROPERTY(QString token READ token NOTIFY preferencesChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect NOTIFY preferencesChanged)
    Q_PROPERTY(bool canConnect READ canConnect NOTIFY preferencesChanged)
    Q_PROPERTY(QString settingsMessage READ settingsMessage NOTIFY settingsMessageChanged)
public:
    /// @brief Создаёт пустую форму без чтения файлов и подключения.
    explicit ClientSettingsModel(QObject* parent = nullptr);
    /// @brief Последние успешно загруженные или сохранённые параметры.
    [[nodiscard]] QString host() const;
    [[nodiscard]] QString port() const;
    [[nodiscard]] QString token() const;
    [[nodiscard]] bool autoConnect() const;
    [[nodiscard]] core::ClientPreferences preferences() const;
    /// @brief Разрешено явное подключение по корректному сохранённому набору.
    [[nodiscard]] bool canConnect() const;
    /// @brief Итог проверки или файловой операции без секретных значений.
    [[nodiscard]] QString settingsMessage() const;
    /// @brief Применяет успешный результат чтения или записи без сетевых действий.
    void applyPreferences(const core::ClientPreferences& preferences);
    /// @brief Принимает результат файловой операции из приложения.
    void setSettingsMessage(const QString& message);
    /// @brief Проверяет форму; saveRequested ещё не означает успешную запись.
    Q_INVOKABLE bool savePreferences(const QString& host, const QString& port, const QString& token, bool autoConnect);
    /// @brief Передаёт явное подключение по сохранённым параметрам.
    Q_INVOKABLE void connectToServer();
signals:
    void preferencesChanged();                                    ///< @brief Изменён сохранённый набор.
    void settingsMessageChanged();                                ///< @brief Изменено пояснение.
    void saveRequested(msa::core::ClientPreferences preferences); ///< @brief Проверенный набор для записи.
    void connectRequested(); ///< @brief Запрос подключения, исполняемый приложением.
private:
    core::ClientPreferences m_preferences{};
    QString m_settingsMessage{};
};
} // namespace msa::ui
