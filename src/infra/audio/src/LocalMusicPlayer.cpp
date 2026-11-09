#include "msa/infra/LocalMusicPlayer.h"

#include "msa/infra/JsonMusicPreferences.h"
#include "msa/infra/detail/MusicLibraryScanner.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>

#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace msa::infra
{
LocalMusicPlayer::LocalMusicPlayer(QString settingsPath, std::uint32_t seed, QObject* parent) :
    QObject{parent}, m_settingsPath{std::move(settingsPath)}, m_playlist{seed}
{
    m_audio.setVolume(static_cast<float>(m_preferences.volume));
    qRegisterMetaType<core::MusicTracks>();
    auto scanner = std::make_unique<MusicLibraryScanner>(m_scanGeneration);
    scanner->moveToThread(&m_scannerThread);
    connect(this, &LocalMusicPlayer::scanRequested, scanner.get(), &MusicLibraryScanner::onScan);
    connect(scanner.get(), &MusicLibraryScanner::scanned, this, &LocalMusicPlayer::onScanned);
    connect(&m_scannerThread, &QThread::finished, scanner.get(), &QObject::deleteLater);
    // После finished удаление выполняет Qt в потоке сканера.
    scanner.release();
    m_scannerThread.start();
}
LocalMusicPlayer::~LocalMusicPlayer()
{
    ++m_scanGeneration;
    m_scannerThread.requestInterruption();
    m_scannerThread.quit();
    m_scannerThread.wait();
    stopPlayer();
}
core::MusicState LocalMusicPlayer::state() const
{
    const auto* track = m_playlist.current();
    const auto error = m_settingsError.isEmpty() ? m_mediaError : m_settingsError + " " + m_mediaError;

    return {m_preferences.folder,
            track ? track->title : std::string{},
            m_playlist.position(),
            m_playlist.count(),
            m_preferences.volume,
            m_player && m_player->playbackState() == QMediaPlayer::PlayingState,
            m_scanning,
            error.trimmed().toStdString()};
}
void LocalMusicPlayer::publish()
{
    emit stateChanged(state());
}
void LocalMusicPlayer::restore()
{
    try
    {
        m_preferences = JsonMusicPreferences::load(m_settingsPath).value_or(core::MusicPreferences{});
        m_settingsError.clear();
    }
    catch (const std::exception&)
    {
        m_preferences = {};
        m_settingsError = "Не удалось прочитать настройки музыки. Выберите папку заново.";
    }
    m_audio.setVolume(static_cast<float>(m_preferences.volume));
    beginScan(false);
}
void LocalMusicPlayer::savePreferences()
{
    try
    {
        const auto directory = QFileInfo{m_settingsPath}.absolutePath();
        if (!QDir{}.mkpath(directory))
        {
            m_settingsError = "Не удалось создать папку настроек музыки.";
            return;
        }
        JsonMusicPreferences::save(m_settingsPath, m_preferences);
        m_settingsError.clear();
    }
    catch (const std::exception&)
    {
        m_settingsError = "Не удалось сохранить настройки музыки.";
    }
}
void LocalMusicPlayer::chooseFolder(const QString& folder)
{
    auto path = folder;
    if (!path.isEmpty() && !QDir::isAbsolutePath(path))
    {
        const QUrl url{path};
        path = url.isLocalFile() ? url.toLocalFile() : QString{};
        if (path.isEmpty() || !QDir::isAbsolutePath(path))
        {
            m_mediaError = "Выберите локальную папку с абсолютным путём.";
            publish();
            return;
        }
    }
    if (path.contains(QChar::Null))
    {
        m_mediaError = "Некорректный путь музыкальной папки.";
        publish();
        return;
    }
    m_preferences.folder = path.isEmpty() ? std::string{} : QDir::cleanPath(path).toStdString();
    savePreferences();
    beginScan(true);
}
void LocalMusicPlayer::beginScan(bool autoplay)
{
    const auto generation = ++m_scanGeneration;
    stopPlayer();
    m_playlist.replace({});
    m_mediaError.clear();
    m_autoplay = autoplay;
    m_scanning = !m_preferences.folder.empty();
    publish();
    if (m_scanning)
    {
        emit scanRequested(QString::fromStdString(m_preferences.folder), generation);
    }
}
void LocalMusicPlayer::onScanned(quint64 generation, core::MusicTracks tracks, const QString& error)
{
    if (generation != m_scanGeneration.load())
    {
        return;
    }
    m_scanning = false;
    m_playlist.replace(std::move(tracks));
    m_mediaError = error;
    if (m_autoplay && m_playlist.current())
    {
        playCurrent();
    }
    publish();
}
void LocalMusicPlayer::stopPlayer()
{
    ++m_loadGeneration;
    m_terminalQueued = false;
    if (!m_player)
    {
        return;
    }
    disconnect(m_player, nullptr, this, nullptr);
    m_player->stop();
    m_player->setAudioOutput(nullptr);
    m_player->deleteLater();
    m_player.clear();
}
void LocalMusicPlayer::playCurrent()
{
    stopPlayer();
    const auto* track = m_playlist.current();
    if (!track)
    {
        return;
    }
    m_mediaError.clear();
    const auto generation = m_loadGeneration;
    m_player = new QMediaPlayer{this};
    m_player->setAudioOutput(&m_audio);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this, generation](QMediaPlayer::PlaybackState)
            {
                if (generation == m_loadGeneration)
                {
                    publish();
                }
            });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this, generation](QMediaPlayer::Error, const QString&) { deferTerminal(true, generation); });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this, generation](QMediaPlayer::MediaStatus status)
            {
                if (status == QMediaPlayer::EndOfMedia)
                {
                    deferTerminal(false, generation);
                }
                if (status == QMediaPlayer::InvalidMedia)
                {
                    deferTerminal(true, generation);
                }
            });
    m_player->setSource(QUrl::fromLocalFile(QString::fromStdString(track->path)));
    m_player->play();
    publish();
}
void LocalMusicPlayer::deferTerminal(bool failed, quint64 generation)
{
    if (generation != m_loadGeneration || m_terminalQueued)
    {
        return;
    }
    m_terminalQueued = true;
    // Не запускаем следующий декодер рекурсивно из сигнала предыдущего.
    QTimer::singleShot(0, this,
                       [this, failed, generation]
                       {
                           if (generation != m_loadGeneration)
                           {
                               return;
                           }
                           const auto advance = failed ? m_playlist.failed() : m_playlist.ended();
                           if (advance)
                           {
                               playCurrent();
                           }
                           else
                           {
                               stopPlayer();
                               m_mediaError =
                                   "Не удалось воспроизвести ни один трек. Проверьте файлы и нажмите «Играть».";
                               publish();
                           }
                       });
}
void LocalMusicPlayer::execute(const core::MusicCommand& command)
{
    if (command.kind == core::MusicCommandKind::ChooseFolder)
    {
        chooseFolder(QString::fromStdString(command.folder));
        return;
    }
    if (command.kind == core::MusicCommandKind::SetVolume)
    {
        if (!std::isfinite(command.volume) || command.volume < 0 || command.volume > 1)
        {
            return;
        }
        m_preferences.volume = command.volume;
        m_audio.setVolume(static_cast<float>(command.volume));
        savePreferences();
        publish();
        return;
    }
    if (command.kind == core::MusicCommandKind::Reshuffle)
    {
        beginScan(true);
        return;
    }
    if (m_scanning || !m_playlist.current())
    {
        return;
    }
    switch (command.kind)
    {
    case core::MusicCommandKind::PlayPause:
        if (m_player && m_player->playbackState() == QMediaPlayer::PlayingState)
        {
            m_player->pause();
        }
        else if (m_player && m_player->playbackState() == QMediaPlayer::PausedState)
        {
            m_player->play();
        }
        else
        {
            m_playlist.retry();
            playCurrent();
        }
        break;
    case core::MusicCommandKind::Next:
        if (m_playlist.next())
        {
            playCurrent();
        }
        break;
    case core::MusicCommandKind::Previous:
        if (m_playlist.previous())
        {
            playCurrent();
        }
        break;
    default:
        break;
    }
    publish();
}
} // namespace msa::infra
