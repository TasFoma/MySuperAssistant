#pragma once

#include "msa/core/MusicData.h"

#include <QObject>
#include <QString>

namespace msa::ui
{
/// @brief Представление локальной музыки: только снимки и явные команды, без сети и аудио.
class MusicModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString positionText READ positionText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(bool canControl READ canControl NOTIFY changed)
    Q_PROPERTY(double volumePercent READ volumePercent WRITE setVolumePercent NOTIFY changed)
    Q_PROPERTY(QString volumeText READ volumeText NOTIFY changed)
public:
    explicit MusicModel(QObject* parent = nullptr);
    /// @brief Применяет снимок без отправки команд обратно проигрывателю.
    void applyState(const core::MusicState& state);
    /// @brief Папка, название, позиция и сообщение об ошибке для панели.
    [[nodiscard]] QString folder() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString positionText() const;
    [[nodiscard]] QString errorText() const;
    /// @brief Состояние воспроизведения и доступность управления независимо от сервера.
    [[nodiscard]] bool playing() const;
    [[nodiscard]] bool scanning() const;
    [[nodiscard]] bool canControl() const;
    /// @brief Громкость в процентах; преобразование в диапазон аудио остаётся в C++.
    [[nodiscard]] double volumePercent() const;
    [[nodiscard]] QString volumeText() const;
    void setVolumePercent(double percent);
    /// @brief Передаёт локальный путь или file URL из диалога в инфраструктуру.
    Q_INVOKABLE void chooseFolder(const QString& folder);
    /// @brief Явные действия пользователя; пауза и переходы не зависят от TCP-сессии.
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void reshuffle();
signals:
    void changed();                                         ///< @brief Обновлён снимок.
    void commandRequested(msa::core::MusicCommand command); ///< @brief Локальная команда связке apps.
private:
    core::MusicState m_state{};
};
} // namespace msa::ui
