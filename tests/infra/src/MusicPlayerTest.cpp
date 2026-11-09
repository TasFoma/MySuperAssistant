#include "msa/desktop/MusicController.h"
#include "msa/infra/JsonMusicPreferences.h"
#include "msa/infra/LocalMusicPlayer.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <functional>
#include <limits>
#include <stdexcept>

namespace
{
constexpr int kWaitMilliseconds = 5000;
constexpr int kQuietMilliseconds = 150;
constexpr std::uint32_t kSeed = 42;
constexpr quint32 kLongWaveMilliseconds = 30000;
constexpr quint32 kShortWaveMilliseconds = 250;
bool waitFor(const std::function<bool()>& predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kWaitMilliseconds)
    {
        QCoreApplication::processEvents();
        if (predicate())
        {
            return true;
        }
        QThread::msleep(1);
    }
    return false;
}
void settle()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kQuietMilliseconds)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}
void writeFile(const QString& path, const QByteArray& bytes = "invalid audio")
{
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(bytes), bytes.size());
}
void writeSilentWave(const QString& path, quint32 durationMilliseconds = kLongWaveMilliseconds)
{
    // Тишина: проверяем настоящий backend, не издавая звук на машине пользователя.
    constexpr quint32 kSampleRate = 8000;
    constexpr quint32 kMillisecondsPerSecond = 1000;
    constexpr quint16 kBytesPerSample = 2;
    const quint32 dataSize = kSampleRate * durationMilliseconds / kMillisecondsPerSecond * kBytesPerSample;
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QDataStream stream{&file};
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32{36 + dataSize};
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32{16} << quint16{1} << quint16{1} << kSampleRate << quint32{kSampleRate * kBytesPerSample}
           << kBytesPerSample << quint16{16};
    stream.writeRawData("data", 4);
    stream << dataSize;
    const QByteArray silence(dataSize, '\0');
    ASSERT_EQ(stream.writeRawData(silence.constData(), silence.size()), silence.size());
}
void choose(msa::infra::LocalMusicPlayer& player, const QString& folder)
{
    player.execute({msa::core::MusicCommandKind::ChooseFolder, folder.toStdString()});
}
TEST(MusicPreferencesTest, MissingFileAndUnicodeRoundTrip)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("music.settings.json");
    EXPECT_FALSE(msa::infra::JsonMusicPreferences::load(path));
    const auto folder = directory.filePath(QString::fromUtf8("Музыка 🙂"));
    msa::infra::JsonMusicPreferences::save(path, {folder.toStdString(), 0.42});
    const auto loaded = msa::infra::JsonMusicPreferences::load(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->folder, folder.toStdString());
    EXPECT_DOUBLE_EQ(loaded->volume, 0.42);
}
TEST(MusicPreferencesTest, InvalidSavePreservesOriginal)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("music.settings.json");
    msa::infra::JsonMusicPreferences::save(path, {"", 0.42});
    for (const auto volume :
         {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        EXPECT_THROW(msa::infra::JsonMusicPreferences::save(path, {"", volume}), std::runtime_error);
    }
    EXPECT_THROW(msa::infra::JsonMusicPreferences::save(path, {"relative", 0.5}), std::runtime_error);
    EXPECT_DOUBLE_EQ(msa::infra::JsonMusicPreferences::load(path)->volume, 0.42);
}
TEST(MusicPreferencesTest, RejectsWrongSchemaAndOversizeWithoutOverwriting)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("music.settings.json");
    for (const auto& bytes :
         {QByteArray{"[]"}, QByteArray{"{"}, QByteArray{R"({"version":2,"folder":"","volume":0.5})"},
          QByteArray{R"({"version":1,"folder":"relative","volume":0.5})"},
          QByteArray{R"({"version":1,"folder":"","volume":"0.5"})"},
          QByteArray{R"({"version":1,"folder":"","volume":2})"},
          QByteArray{R"({"version":1,"folder":"","volume":0.5,"extra":0})"}, QByteArray(65537, ' ')})
    {
        writeFile(path, bytes);
        EXPECT_THROW(static_cast<void>(msa::infra::JsonMusicPreferences::load(path)), std::runtime_error);
        QFile file{path};
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), bytes);
    }
}
TEST(MusicPlayerTest, RestoresNestedFormatsWithoutAutoplayAndWithoutNetwork)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto folder = directory.filePath(QString::fromUtf8("Музыка"));
    ASSERT_TRUE(QDir{}.mkpath(folder + "/nested"));
    for (const auto* suffix : {"mp3", "FLAC", "wav", "m4a", "ogg", "wma"})
    {
        writeFile(folder + "/nested/track." + suffix);
    }
    writeFile(folder + "/ignored.txt");
    const auto path = directory.filePath("music.settings.json");
    msa::infra::JsonMusicPreferences::save(path, {folder.toStdString(), 0.42});
    msa::ui::MusicModel model;
    msa::infra::LocalMusicPlayer player{path, kSeed};
    msa::desktop::MusicController controller{model, player};
    controller.restore();
    ASSERT_TRUE(waitFor([&player] { return !player.state().scanning; }));
    EXPECT_EQ(player.state().count, 6);
    EXPECT_EQ(player.state().position, 1);
    EXPECT_FALSE(player.state().playing);
    EXPECT_TRUE(player.state().error.empty());
    EXPECT_TRUE(model.canControl());
    EXPECT_EQ(model.volumePercent(), 42);
    EXPECT_FALSE(model.playing());
}
TEST(MusicPlayerTest, BadFilesStopAfterOnePassAndExplicitRetryStopsAgain)
{
    QTemporaryDir directory;
    const auto folder = directory.filePath("bad");
    ASSERT_TRUE(QDir{}.mkpath(folder));
    writeFile(folder + "/bad.mp3");
    writeFile(folder + "/bad.wav");
    msa::infra::LocalMusicPlayer player{directory.filePath("music.settings.json"), kSeed};
    choose(player, folder);
    ASSERT_TRUE(waitFor([&player] { return !player.state().scanning && !player.state().error.empty(); }));
    EXPECT_EQ(player.state().count, 2);
    EXPECT_FALSE(player.state().playing);
    const auto position = player.state().position;
    settle();
    EXPECT_EQ(player.state().position, position);
    EXPECT_FALSE(player.state().playing);
    player.execute({msa::core::MusicCommandKind::PlayPause});
    ASSERT_TRUE(waitFor([&player] { return !player.state().error.empty(); }));
    EXPECT_FALSE(player.state().playing);
}
TEST(MusicPlayerTest, EmptyFolderStopsPreviousPlaybackAndNewSelectionWins)
{
    QTemporaryDir directory;
    const auto music = directory.filePath("music");
    const auto empty = directory.filePath("empty");
    ASSERT_TRUE(QDir{}.mkpath(music));
    ASSERT_TRUE(QDir{}.mkpath(empty));
    writeSilentWave(music + "/silent.wav");
    msa::infra::LocalMusicPlayer player{directory.filePath("music.settings.json"), kSeed};
    player.execute({msa::core::MusicCommandKind::SetVolume, {}, 0});
    choose(player, QUrl::fromLocalFile(music).toString());
    ASSERT_TRUE(waitFor([&player] { return player.state().playing; }));
    settle();
    EXPECT_TRUE(player.state().playing);
    EXPECT_TRUE(player.state().error.empty());
    player.execute({msa::core::MusicCommandKind::PlayPause});
    EXPECT_FALSE(player.state().playing);
    player.execute({msa::core::MusicCommandKind::Next});
    ASSERT_TRUE(waitFor([&player] { return player.state().playing; }));
    player.execute({msa::core::MusicCommandKind::Previous});
    ASSERT_TRUE(waitFor([&player] { return player.state().playing; }));
    choose(player, empty);
    EXPECT_FALSE(player.state().playing);
    ASSERT_TRUE(waitFor([&player] { return !player.state().scanning; }));
    EXPECT_EQ(player.state().count, 0);
    EXPECT_FALSE(player.state().error.empty());
    choose(player, music);
    choose(player, directory.filePath("missing"));
    ASSERT_TRUE(waitFor([&player] { return !player.state().scanning; }));
    settle();
    EXPECT_EQ(player.state().count, 0);
    EXPECT_FALSE(player.state().playing);
    choose(player, "");
    EXPECT_EQ(player.state().count, 0);
    EXPECT_TRUE(player.state().folder.empty());
    EXPECT_TRUE(player.state().error.empty());
}
TEST(MusicPlayerTest, ActualEndOfMediaAdvancesAndWrapsWithoutUserCommand)
{
    QTemporaryDir directory;
    const auto folder = directory.filePath("short");
    ASSERT_TRUE(QDir{}.mkpath(folder));
    writeSilentWave(folder + "/first.wav", kShortWaveMilliseconds);
    writeSilentWave(folder + "/second.wav", kShortWaveMilliseconds);
    msa::infra::LocalMusicPlayer player{directory.filePath("music.settings.json"), kSeed};
    player.execute({msa::core::MusicCommandKind::SetVolume, {}, 0});
    choose(player, folder);
    ASSERT_TRUE(waitFor([&player] { return !player.state().scanning && player.state().count == 2; }));
    EXPECT_EQ(player.state().position, 1);
    ASSERT_TRUE(waitFor([&player] { return player.state().position == 2; }));
    ASSERT_TRUE(waitFor([&player] { return player.state().position == 1; }));
    EXPECT_TRUE(player.state().error.empty());
    choose(player, "");
}
TEST(MusicPlayerTest, InvalidPreferencesRemainUntouchedUntilExplicitVolumeSave)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("music.settings.json");
    writeFile(path, "damaged");
    msa::ui::MusicModel model;
    msa::infra::LocalMusicPlayer player{path, kSeed};
    msa::desktop::MusicController controller{model, player};
    controller.restore();
    EXPECT_FALSE(model.errorText().isEmpty());
    EXPECT_FALSE(model.canControl());
    QFile original{path};
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), "damaged");
    original.close();
    model.setVolumePercent(25);
    EXPECT_EQ(model.volumePercent(), 25);
    EXPECT_DOUBLE_EQ(msa::infra::JsonMusicPreferences::load(path)->volume, 0.25);
    model.setVolumePercent(-1);
    model.setVolumePercent(std::numeric_limits<double>::infinity());
    EXPECT_EQ(model.volumePercent(), 25);
    choose(player, "relative");
    EXPECT_TRUE(player.state().folder.empty());
    EXPECT_FALSE(player.state().error.empty());
}
} // namespace
