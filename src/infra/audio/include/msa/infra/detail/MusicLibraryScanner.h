#pragma once

#include "msa/core/MusicData.h"

#include <QMetaType>
#include <QObject>
#include <QString>

#include <atomic>

namespace msa::infra
{
/// @brief Внутренний рабочий объект: читает только выбранную локальную папку.
class MusicLibraryScanner final : public QObject
{
    Q_OBJECT
public:
    /// @brief Счётчик живёт до завершения потока; отменяет устаревшую работу между файлами.
    explicit MusicLibraryScanner(std::atomic<quint64>& generation);
signals:
    void scanned(quint64 generation, msa::core::MusicTracks tracks, QString error);
public slots:
    /// @brief Рекурсивный поиск поддержанных расширений без перехода по ссылкам на папки.
    void onScan(const QString& folder, quint64 generation);

private:
    std::atomic<quint64>& m_generation;
};
} // namespace msa::infra
