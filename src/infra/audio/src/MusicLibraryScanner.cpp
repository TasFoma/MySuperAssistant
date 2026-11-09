#include "msa/infra/detail/MusicLibraryScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <utility>

namespace msa::infra
{
MusicLibraryScanner::MusicLibraryScanner(std::atomic<quint64>& generation) : m_generation{generation} {}
void MusicLibraryScanner::onScan(const QString& folder, quint64 generation)
{
    if (generation != m_generation.load())
    {
        return;
    }
    const QFileInfo directory{folder};
    if (folder.isEmpty() || !directory.isDir() || !directory.isReadable())
    {
        emit scanned(generation, {}, "Музыкальная папка недоступна.");
        return;
    }
    const QStringList extensions{"mp3", "flac", "wav", "m4a", "ogg", "wma"};
    core::MusicTracks tracks;
    QDirIterator iterator{folder, QDir::Files, QDirIterator::Subdirectories};
    while (iterator.hasNext())
    {
        if (generation != m_generation.load() || QThread::currentThread()->isInterruptionRequested())
        {
            return;
        }
        iterator.next();
        const auto info = iterator.fileInfo();
        if (extensions.contains(info.suffix().toLower()))
        {
            tracks.push_back({info.absoluteFilePath().toStdString(), info.completeBaseName().toStdString()});
        }
    }
    std::ranges::sort(tracks, {}, &core::MusicTrack::path);
    const auto empty = tracks.empty();
    emit scanned(generation, std::move(tracks), empty ? "В папке нет поддерживаемых аудиофайлов." : QString{});
}
} // namespace msa::infra
