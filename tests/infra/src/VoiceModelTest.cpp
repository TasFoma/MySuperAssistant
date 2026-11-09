#include "msa/desktop/SpeechController.h"
#include "msa/desktop/VoiceController.h"
#include "msa/infra/JsonVoicePreferences.h"
#include "msa/infra/QtSpeechSynthesizer.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <stdexcept>
#include <utility>

namespace
{
using State = msa::core::RecognitionState;
using Kind = msa::core::RecognitionEvent::Kind;
class FakeSpeechRecognizer final : public msa::core::SpeechRecognizer
{
public:
    void setHandler(msa::core::RecognitionHandler handler) override;
    void configure(const std::string& modelPath) override;
    void start() override;
    void finish() override;
    void cancel() override;
    void shutdown() override;
    [[nodiscard]] State state() const override;
    int m_starts{};
    int m_configures{};

private:
    void update(State state);
    msa::core::RecognitionHandler m_handler{};
    State m_state{State::Unconfigured};
};
void FakeSpeechRecognizer::setHandler(msa::core::RecognitionHandler handler)
{
    m_handler = std::move(handler);
}
State FakeSpeechRecognizer::state() const
{
    return m_state;
}
void FakeSpeechRecognizer::update(State state)
{
    m_state = state;
    if (m_handler)
    {
        m_handler({Kind::State, state, {}});
    }
}
void FakeSpeechRecognizer::configure(const std::string& path)
{
    ++m_configures;
    update(path.empty() ? State::Unconfigured : State::Idle);
}
void FakeSpeechRecognizer::start()
{
    ++m_starts;
    update(State::Recording);
}
void FakeSpeechRecognizer::finish()
{
    if (m_handler)
    {
        m_handler({Kind::Text, State::Finishing, "Распознанный текст <b>🙂</b>"});
    }
    update(State::Ready);
}
void FakeSpeechRecognizer::cancel()
{
    update(State::Idle);
}
void FakeSpeechRecognizer::shutdown()
{
    update(State::Idle);
}
class FakeSpeechSynthesizer final : public msa::core::SpeechSynthesizer
{
public:
    void setHandler(msa::core::SpeechHandler handler) override;
    void speak(const std::string& text) override;
    void stop() override;
    msa::core::SpeechHandler m_handler{};
    std::string m_text{};
    bool m_speaking{false};
};
void FakeSpeechSynthesizer::setHandler(msa::core::SpeechHandler handler)
{
    m_handler = std::move(handler);
}
void FakeSpeechSynthesizer::speak(const std::string& text)
{
    m_text = text;
    m_speaking = true;
    if (m_handler)
    {
        m_handler({msa::core::SpeechState::Speaking, {}});
    }
}
void FakeSpeechSynthesizer::stop()
{
    m_speaking = false;
    if (m_handler)
    {
        m_handler({msa::core::SpeechState::Idle, {}});
    }
}
TEST(VoiceModelTest, InputMeterFollowsRecordingAndResetsOnCompletion)
{
    msa::ui::VoiceModel model;
    model.applyEvent({Kind::Level, State::Recording, {}, 0.75});
    EXPECT_DOUBLE_EQ(model.inputLevel(), 0.75);
    EXPECT_TRUE(model.canFinish());
    model.applyEvent({Kind::State, State::Ready, {}});
    EXPECT_DOUBLE_EQ(model.inputLevel(), 0);
    EXPECT_TRUE(model.draft().isEmpty());
}
TEST(SpeechSynthesisTest, ExplicitPlaybackAndRecordingExcludeEachOtherWithoutChangingDraft)
{
    QTemporaryDir directory;
    FakeSpeechRecognizer recognizer;
    FakeSpeechSynthesizer synthesizer;
    msa::ui::VoiceModel voice;
    msa::desktop::SpeechController speech{voice, synthesizer, recognizer};
    msa::desktop::VoiceController controller{voice, recognizer, directory.filePath("voice.settings.json")};
    voice.chooseModel(directory.filePath("model"));
    voice.setDraft("Мой черновик");
    EXPECT_FALSE(synthesizer.m_speaking);
    voice.speak("Ответ <b>🙂</b>");
    EXPECT_TRUE(voice.speaking());
    EXPECT_EQ(synthesizer.m_text, "Ответ <b>🙂</b>");
    voice.start();
    EXPECT_FALSE(synthesizer.m_speaking);
    EXPECT_FALSE(voice.speaking());
    EXPECT_EQ(recognizer.state(), State::Recording);
    voice.speak("Следующий ответ");
    EXPECT_EQ(recognizer.state(), State::Idle);
    EXPECT_TRUE(synthesizer.m_speaking);
    EXPECT_EQ(synthesizer.m_text, "Следующий ответ");
    voice.stopSpeech();
    EXPECT_FALSE(voice.speaking());
    EXPECT_EQ(voice.draft(), QString::fromUtf8("Мой черновик"));
    synthesizer.m_handler({msa::core::SpeechState::Failed, "Нет системного голоса"});
    EXPECT_FALSE(voice.errorText().isEmpty());
    EXPECT_TRUE(voice.canStart());
}
TEST(SpeechSynthesisTest, ControllerDestructionStopsSpeechAndUnsubscribes)
{
    FakeSpeechRecognizer recognizer;
    FakeSpeechSynthesizer synthesizer;
    msa::ui::VoiceModel voice;
    {
        msa::desktop::SpeechController controller{voice, synthesizer, recognizer};
        voice.speak("Проверка");
        ASSERT_TRUE(synthesizer.m_speaking);
    }
    EXPECT_FALSE(synthesizer.m_speaking);
    EXPECT_FALSE(synthesizer.m_handler);
}
TEST(SpeechSynthesisTest, MissingRealQtEngineAndInvalidInputFailWithoutAudioOrShell)
{
    msa::infra::QtSpeechSynthesizer synthesizer{"msa_missing_test_engine"};
    msa::core::SpeechEvent event;
    synthesizer.setHandler([&event](const msa::core::SpeechEvent& value) { event = value; });
    synthesizer.speak("  ");
    EXPECT_EQ(event.state, msa::core::SpeechState::Failed);
    EXPECT_FALSE(event.error.empty());
    synthesizer.speak("Тест 🙂");
    EXPECT_EQ(event.state, msa::core::SpeechState::Failed);
    EXPECT_FALSE(event.error.empty());
    synthesizer.stop();
    QCoreApplication::processEvents();
    EXPECT_EQ(event.state, msa::core::SpeechState::Failed);
}
TEST(VoiceModelTest, OfflineRecognitionAppendsDraftAndReusesSavedModel)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("voice.settings.json");
    FakeSpeechRecognizer recognizer;
    msa::ui::VoiceModel voice;
    msa::desktop::VoiceController controller{voice, recognizer, path};
    controller.restore();
    EXPECT_FALSE(voice.canStart());
    EXPECT_FALSE(QFile::exists(path));
    voice.chooseModel(QUrl::fromLocalFile(directory.path()).toString());
    EXPECT_EQ(voice.modelPath(), directory.path());
    EXPECT_TRUE(voice.canStart());
    EXPECT_EQ(recognizer.m_starts, 0);
    voice.setDraft("Ранее введённый текст");
    voice.start();
    voice.start();
    EXPECT_EQ(recognizer.m_starts, 1);
    EXPECT_TRUE(voice.canFinish());
    voice.finish();
    EXPECT_EQ(voice.draft(), QString::fromUtf8("Ранее введённый текст\nРаспознанный текст <b>🙂</b>"));
    EXPECT_TRUE(voice.canStart());
    voice.start();
    voice.cancel();
    EXPECT_FALSE(voice.canCancel());
    EXPECT_EQ(recognizer.m_starts, 2);
    const auto draft = voice.draft();
    voice.chooseModel("https://example.invalid/model");
    EXPECT_FALSE(voice.errorText().isEmpty());
    EXPECT_EQ(voice.draft(), draft);
    EXPECT_EQ(msa::infra::JsonVoicePreferences::load(path).value(), directory.path());
}
TEST(VoiceModelTest, BrokenFileAndWriteFailurePreserveSettingsAndDoNotStartRecording)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("voice.settings.json");
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("damaged");
    file.close();
    FakeSpeechRecognizer recognizer;
    msa::ui::VoiceModel voice;
    msa::desktop::VoiceController controller{voice, recognizer, path};
    controller.restore();
    EXPECT_EQ(recognizer.m_configures, 0);
    EXPECT_EQ(recognizer.m_starts, 0);
    EXPECT_FALSE(voice.errorText().isEmpty());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), "damaged");
    file.close();
    voice.chooseModel(directory.path());
    EXPECT_TRUE(voice.errorText().isEmpty());
    EXPECT_EQ(recognizer.m_starts, 0);
    msa::ui::VoiceModel failed;
    failed.applyModelPath(directory.path());
    FakeSpeechRecognizer failedRecognizer;
    msa::desktop::VoiceController failedController{failed, failedRecognizer, directory.path()};
    failed.chooseModel(directory.filePath("another"));
    EXPECT_EQ(failed.modelPath(), directory.path());
    EXPECT_EQ(failedRecognizer.m_configures, 0);
    EXPECT_FALSE(failed.errorText().isEmpty());
}
TEST(VoicePreferencesTest, StrictSchemaRoundTripsUnicodeAndRejectsInvalidSaveWithoutOverwrite)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("voice.settings.json");
    const auto model = directory.filePath(QString::fromUtf8("Модель 🙂"));
    EXPECT_FALSE(msa::infra::JsonVoicePreferences::load(path));
    msa::infra::JsonVoicePreferences::save(path, model);
    EXPECT_EQ(msa::infra::JsonVoicePreferences::load(path).value(), model);
    EXPECT_THROW(msa::infra::JsonVoicePreferences::save(path, "relative"), std::runtime_error);
    EXPECT_EQ(msa::infra::JsonVoicePreferences::load(path).value(), model);
    for (const auto& bytes :
         {QByteArray{"[]"}, QByteArray{"{"}, QByteArray{R"({"version":2,"model_path":""})"},
          QByteArray{R"({"version":1,"model_path":"relative"})"}, QByteArray{R"({"version":1,"model_path":12})"},
          QByteArray{R"({"version":1,"model_path":"","extra":0})"}, QByteArray(65537, ' ')})
    {
        QFile file{path};
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();
        EXPECT_THROW(static_cast<void>(msa::infra::JsonVoicePreferences::load(path)), std::runtime_error);
    }
}
} // namespace
