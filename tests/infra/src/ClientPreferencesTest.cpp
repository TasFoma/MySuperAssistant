#include "msa/desktop/DesktopPreferencesController.h"
#include "msa/infra/JsonClientPreferences.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/DesktopState.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>
#include <stdexcept>

namespace
{
TEST(ClientPreferencesTest, MissingFileKeepsOfflineFirstConfigurationAndDefaultAutoConnect)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("client.settings.json");
    EXPECT_FALSE(msa::infra::JsonClientPreferences::load(path).has_value());
    msa::ui::ClientModels models;
    msa::ui::DesktopState state{models};
    msa::desktop::DesktopPreferencesController controller{state, path};
    controller.load();
    EXPECT_EQ(state.page(), 3);
    EXPECT_TRUE(state.autoConnect());
    EXPECT_TRUE(state.host().isEmpty());
    EXPECT_FALSE(QFile::exists(path));
    EXPECT_FALSE(models.online());
}

TEST(ClientPreferencesTest, RoundTripsSettingsAndDisabledAutoConnectWithoutChangingToken)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath("client.settings.json");
    const msa::core::ClientPreferences preferences{"::1", 43210, " токен + % 🙂 ", false};
    msa::infra::JsonClientPreferences::save(path, preferences);
    const auto loaded = msa::infra::JsonClientPreferences::load(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->host, preferences.host);
    EXPECT_EQ(loaded->port, preferences.port);
    EXPECT_EQ(loaded->token, preferences.token);
    EXPECT_FALSE(loaded->autoConnect);
}

TEST(ClientPreferencesTest, InvalidSaveLeavesOriginalFileUnchanged)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("client.settings.json");
    msa::infra::JsonClientPreferences::save(path, {"localhost", 43210, "original", true});
    EXPECT_THROW(msa::infra::JsonClientPreferences::save(path, {"http://localhost", 0, "secret", true}),
                 std::runtime_error);
    EXPECT_EQ(msa::infra::JsonClientPreferences::load(path)->token, "original");
}

TEST(ClientPreferencesTest, RejectsMalformedSchemaAndDoesNotDiscloseSecretsOrOverwriteFile)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("client.settings.json");
    for (const auto& bytes :
         {QByteArray{"[]"}, QByteArray{"{"},
          QByteArray{"{\"version\":1,\"host\":\"localhost\",\"port\":12.5,\"token\":\"secret\",\"auto_connect\":true}"},
          QByteArray{"{\"version\":2,\"host\":\"localhost\",\"port\":1234,\"token\":\"secret\",\"auto_connect\":true}"},
          QByteArray{"{\"version\":1,\"host\":\"localhost\",\"port\":1234,\"token\":\"secret\",\"auto_connect\":true,"
                     "\"extra\":0}"}})
    {
        QFile file{path};
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();
        msa::ui::ClientModels models;
        msa::ui::DesktopState state{models};
        msa::desktop::DesktopPreferencesController controller{state, path};
        controller.load();
        EXPECT_EQ(state.page(), 3);
        EXPECT_FALSE(state.settingsMessage().contains("secret"));
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), bytes);
    }
}

TEST(ClientPreferencesTest, ValidatesFormAndPersistsThroughApplicationBinding)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("nested/client.settings.json");
    msa::ui::ClientModels models;
    msa::ui::DesktopState state{models};
    msa::desktop::DesktopPreferencesController controller{state, path};
    for (const auto* port : {"", "0", "65536", "1.5", "+1234", "-1"})
    {
        EXPECT_FALSE(state.savePreferences("localhost", port, "secret", false));
        EXPECT_FALSE(QFile::exists(path));
    }
    EXPECT_FALSE(state.savePreferences("http://localhost", "1234", "secret", false));
    EXPECT_FALSE(state.savePreferences("localhost", "1234", " ", false));
    EXPECT_FALSE(state.savePreferences("localhost", "1234", QString::fromUtf8(" 　"), false));
    EXPECT_TRUE(state.savePreferences(" localhost ", "65535", " secret ", false));
    EXPECT_EQ(state.host(), "localhost");
    EXPECT_EQ(state.token(), " secret ");
    EXPECT_EQ(state.port(), "65535");
    EXPECT_FALSE(state.autoConnect());
    EXPECT_FALSE(models.online());
    EXPECT_FALSE(msa::infra::JsonClientPreferences::load(path)->autoConnect);
}

TEST(ClientPreferencesTest, WriteFailureKeepsPreviouslyAppliedSettings)
{
    QTemporaryDir directory;
    msa::ui::ClientModels models;
    msa::ui::DesktopState state{models};
    state.applyPreferences({"previous", 1234, "old", true});
    msa::desktop::DesktopPreferencesController controller{state, directory.path()};
    EXPECT_TRUE(state.savePreferences("changed", "5678", "secret", false));
    EXPECT_EQ(state.host(), "previous");
    EXPECT_EQ(state.token(), "old");
    EXPECT_FALSE(state.settingsMessage().contains("secret"));
}
} // namespace
