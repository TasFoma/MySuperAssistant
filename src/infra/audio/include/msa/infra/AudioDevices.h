#pragma once

#include "msa/core/AudioDeviceData.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QObject>
#include <QString>

namespace msa::infra
{
/// @brief Перечисляет устройства и сохраняет выбор, не открывая микрофон или динамики.
class AudioDevices final : public QObject
{
    Q_OBJECT
public:
    explicit AudioDevices(QString settingsPath, QObject* parent = nullptr);
    /// @brief Восстанавливает выбор, не перезаписывая повреждённый файл.
    void restore();
    /// @brief Обновляет каталог, в том числе после подключения/отключения устройства.
    void refresh();
    /// @brief Сохраняет доступный ID либо пустой системный выбор; ошибка оставляет прежние значения.
    void select(bool output, const QString& identifier);
    [[nodiscard]] core::AudioDevicesState state() const;
    /// @brief Возвращает null-устройство, если явно выбранное устройство отсутствует.
    [[nodiscard]] QAudioDevice outputDevice() const;
signals:
    void changed(); ///< @brief Каталог, выбор или ошибка изменены; устройства не открывались.
private:
    QString m_settingsPath;
    QMediaDevices m_devices{this};
    core::AudioDevicePreferences m_preferences{};
    QString m_error{};
};
} // namespace msa::infra
