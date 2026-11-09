#pragma once

#include "msa/core/AudioDeviceData.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace msa::ui
{
/// @brief Каталог и действия аудиоустройств без доступа к оборудованию или файлам.
class AudioDevicesModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList inputNames READ inputNames NOTIFY changed)
    Q_PROPERTY(QStringList outputNames READ outputNames NOTIFY changed)
    Q_PROPERTY(int inputIndex READ inputIndex NOTIFY changed)
    Q_PROPERTY(int outputIndex READ outputIndex NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
    explicit AudioDevicesModel(QObject* parent = nullptr);
    /// @brief Применяет снимок без испускания команд выбора.
    void applyState(const core::AudioDevicesState& state);
    [[nodiscard]] QStringList inputNames() const;
    [[nodiscard]] QStringList outputNames() const;
    [[nodiscard]] int inputIndex() const;
    [[nodiscard]] int outputIndex() const;
    [[nodiscard]] QString errorText() const;
    /// @brief Запрашивает сохранение доступного устройства по индексу снимка.
    Q_INVOKABLE void selectInput(int index);
    Q_INVOKABLE void selectOutput(int index);
    /// @brief Запрашивает перечитывание каталога, не включая запись.
    Q_INVOKABLE void refresh();
signals:
    void changed();                                           ///< @brief Применён новый каталог/выбор/диагностика.
    void selectionRequested(bool output, QString identifier); ///< @brief Выбран устойчивый ID, а не имя.
    void refreshRequested();                                  ///< @brief Обновить перечень устройств.
private:
    void select(bool output, int index);

private:
    core::AudioDevicesState m_state{};
};
} // namespace msa::ui
