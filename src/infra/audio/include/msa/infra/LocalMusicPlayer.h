#pragma once

#include "msa/core/MusicData.h"
#include "msa/core/MusicPlaylist.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <atomic>
#include <cstdint>

namespace msa::infra
{
/// @brief Локальное аудио и фоновое сканирование, независимые от TCP-сессии.
class LocalMusicPlayer final : public QObject
{
    Q_OBJECT
public:
    /// @brief Принимает отдельный файл параметров и seed очереди; ничего не запускает.
    LocalMusicPlayer(QString settingsPath, std::uint32_t seed, QObject* parent = nullptr);
    ~LocalMusicPlayer() override;
    /// @brief Текущий снимок для модели представления.
    [[nodiscard]] core::MusicState state() const;
    /// @brief Восстанавливает папку и громкость без автоматического воспроизведения.
    void restore();
    /// @brief Выполняет явное действие пользователя в потоке владельца.
    void execute(const core::MusicCommand& command);
    /// @brief Меняет только вывод; недоступный выбор останавливает звук без скрытой подмены.
    void setOutputDevice(const QAudioDevice& device);
signals:
    void stateChanged(msa::core::MusicState state);         ///< @brief Новый снимок плеера.
    void scanRequested(QString folder, quint64 generation); ///< @brief Запрос рабочему потоку.
private slots:
    void onScanned(quint64 generation, msa::core::MusicTracks tracks, const QString& error);

private:
    void chooseFolder(const QString& folder);
    void beginScan(bool autoplay);
    void savePreferences();
    void stopPlayer();
    void playCurrent();
    void deferTerminal(bool failed, quint64 generation);
    void publish();

private:
    QString m_settingsPath;
    core::MusicPreferences m_preferences{};
    core::MusicPlaylist m_playlist;
    QAudioOutput m_audio{this};
    QPointer<QMediaPlayer> m_player{};
    std::atomic<quint64> m_scanGeneration{};
    QThread m_scannerThread{};
    quint64 m_loadGeneration{};
    bool m_scanning{false};
    bool m_autoplay{false};
    bool m_terminalQueued{false};
    QString m_settingsError{};
    QString m_mediaError{};
    bool m_outputAvailable{true};
};
} // namespace msa::infra
