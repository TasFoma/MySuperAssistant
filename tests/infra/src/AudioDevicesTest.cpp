#include "msa/infra/AudioDevices.h"
#include "msa/infra/JsonAudioDevicePreferences.h"
#include "msa/infra/LocalMusicPlayer.h"
#include "msa/infra/QtSpeechSynthesizer.h"
#include "msa/ui/AudioDevicesModel.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
TEST(AudioDevicePreferencesTest, RoundTripsOpaqueIdsAndPreservesFileOnInvalidSave)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("audio.json");
    EXPECT_FALSE(msa::infra::JsonAudioDevicePreferences::load(path));
    const msa::core::AudioDevicePreferences preferences{QByteArray{"mic\0id", 6}.toBase64().toStdString(), ""};
    msa::infra::JsonAudioDevicePreferences::save(path, preferences);
    EXPECT_EQ(msa::infra::JsonAudioDevicePreferences::load(path)->input, preferences.input);
    EXPECT_TRUE(msa::infra::JsonAudioDevicePreferences::load(path)->output.empty());
    EXPECT_THROW(msa::infra::JsonAudioDevicePreferences::save(path, {"not base64!", {}}), std::runtime_error);
    EXPECT_EQ(msa::infra::JsonAudioDevicePreferences::load(path)->input, preferences.input);
    EXPECT_FALSE(msa::infra::JsonAudioDevicePreferences::validIdentifier(QString(4097, 'A')));
    EXPECT_FALSE(msa::infra::JsonAudioDevicePreferences::validIdentifier("YQ"));
    EXPECT_TRUE(msa::infra::JsonAudioDevicePreferences::validIdentifier("YQ=="));
}
TEST(AudioDevicePreferencesTest, RejectsBrokenSchemaWithoutOverwriting)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("audio.json");
    for (const auto& bytes :
         {QByteArray{"{"}, QByteArray{"[]"}, QByteArray{R"({"version":2,"input_device":"","output_device":""})"},
          QByteArray{R"({"version":1,"input_device":12,"output_device":""})"},
          QByteArray{R"({"version":1,"input_device":"","output_device":"","extra":0})"}, QByteArray(16385, ' ')})
    {
        QFile file{path};
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(bytes), bytes.size());
        file.close();
        EXPECT_THROW(static_cast<void>(msa::infra::JsonAudioDevicePreferences::load(path)), std::runtime_error);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), bytes);
    }
}
TEST(AudioDevicesTest, MissingDeviceRemainsSelectedAndRefreshNeverRewritesSettings)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("audio.json");
    const auto missing = QByteArray{"msa-missing-device"}.toBase64().toStdString();
    msa::infra::JsonAudioDevicePreferences::save(path, {missing, missing});
    msa::infra::AudioDevices devices{path};
    devices.restore();
    const auto state = devices.state();
    EXPECT_EQ(state.preferences.input, missing);
    EXPECT_EQ(state.preferences.output, missing);
    ASSERT_FALSE(state.inputs.empty());
    EXPECT_FALSE(state.inputs.back().available);
    EXPECT_FALSE(state.outputs.back().available);
    EXPECT_TRUE(devices.outputDevice().isNull());
    devices.refresh();
    devices.select(false, QString::fromStdString(missing));
    EXPECT_FALSE(devices.state().error.empty());
    EXPECT_EQ(msa::infra::JsonAudioDevicePreferences::load(path)->input, missing);
    devices.select(false, {});
    EXPECT_TRUE(devices.state().preferences.input.empty());
    EXPECT_EQ(devices.state().preferences.output, missing);
    EXPECT_TRUE(msa::infra::JsonAudioDevicePreferences::load(path)->input.empty());
}
TEST(AudioDevicesTest, FileFailureKeepsSelectionAndDoesNotOpenDevices)
{
    QTemporaryDir directory;
    QFile blocker{directory.filePath("blocker")};
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    msa::infra::AudioDevices devices{directory.filePath("blocker/audio.json")};
    devices.restore();
    devices.select(false, {});
    EXPECT_FALSE(devices.state().error.empty());
    EXPECT_TRUE(devices.state().preferences.input.empty());
}
TEST(AudioDevicesModelTest, StableIdsSurviveDuplicateNamesAndCatalogReorderingWithoutCommands)
{
    msa::ui::AudioDevicesModel model;
    msa::core::AudioDevicesState state;
    state.inputs = {{"", "По умолчанию", true},
                    {"YQ==", "Одинаковое имя", true},
                    {"Yg==", "Одинаковое имя", true},
                    {"Yw==", "Недоступно", false}};
    state.outputs = {{"", "По умолчанию", true}, {"ZA==", "Наушники", true}};
    state.preferences.input = "Yg==";
    std::vector<QString> identifiers;
    int refreshes{};
    QObject::connect(&model, &msa::ui::AudioDevicesModel::selectionRequested, &model,
                     [&identifiers](bool, const QString& identifier) { identifiers.push_back(identifier); });
    QObject::connect(&model, &msa::ui::AudioDevicesModel::refreshRequested, &model, [&refreshes] { ++refreshes; });
    model.applyState(state);
    EXPECT_TRUE(identifiers.empty());
    EXPECT_EQ(model.inputIndex(), 2);
    model.selectInput(1);
    ASSERT_EQ(identifiers.size(), 1U);
    EXPECT_EQ(identifiers.back(), "YQ==");
    model.selectInput(3);
    model.selectInput(-1);
    model.selectOutput(20);
    EXPECT_EQ(identifiers.size(), 1U);
    std::swap(state.inputs[1], state.inputs[2]);
    model.applyState(state);
    EXPECT_EQ(model.inputIndex(), 1);
    EXPECT_EQ(identifiers.size(), 1U);
    model.selectOutput(1);
    EXPECT_EQ(identifiers.back(), "ZA==");
    model.refresh();
    EXPECT_EQ(refreshes, 1);
}
TEST(AudioDevicesTest, UnavailableOutputStopsMusicAndRejectsSpeechWithoutFallbackOrSound)
{
    QTemporaryDir directory;
    msa::infra::LocalMusicPlayer player{directory.filePath("music.json"), 42};
    player.setOutputDevice({});
    player.execute({msa::core::MusicCommandKind::PlayPause});
    EXPECT_FALSE(player.state().playing);
    EXPECT_FALSE(player.state().error.empty());
    msa::infra::QtSpeechSynthesizer speech{"msa-missing-engine"};
    msa::core::SpeechEvent event;
    speech.setHandler([&event](const msa::core::SpeechEvent& value) { event = value; });
    speech.setOutputDevice({}, false);
    speech.speak("Не должно прозвучать");
    EXPECT_EQ(event.state, msa::core::SpeechState::Failed);
    EXPECT_NE(event.error.find("вывода"), std::string::npos);
}
} // namespace
