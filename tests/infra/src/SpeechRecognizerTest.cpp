#include "msa/infra/ProcessSpeechRecognizer.h"
#include "msa/infra/VoiceProtocol.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using State = msa::core::RecognitionState;
using Kind = msa::core::RecognitionEvent::Kind;
constexpr int kWaitMilliseconds = 6000;
constexpr int kQuietMilliseconds = 200;
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
TEST(VoiceProtocolTest, PreservesSplitUtf8MultipleFramesAndLargeIdentifiers)
{
    QByteArray pending;
    const auto first = msa::infra::VoiceProtocol::encode(
        {{"type", "text"}, {"session", "18446744073709551615"}, {"text", "Русский текст 🙂"}});
    const auto split = first.indexOf(QString::fromUtf8("🙂").toUtf8()) + 1;
    EXPECT_TRUE(msa::infra::VoiceProtocol::consume(pending, first.first(split)).isEmpty());
    const auto messages = msa::infra::VoiceProtocol::consume(
        pending, first.sliced(split) + msa::infra::VoiceProtocol::encode({{"type", "done"}, {"session", "1"}}));
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages.front().value("text").toString(), QString::fromUtf8("Русский текст 🙂"));
    EXPECT_EQ(msa::infra::VoiceProtocol::session(messages.front()), std::numeric_limits<quint64>::max());
    EXPECT_TRUE(pending.isEmpty());
}
TEST(VoiceProtocolTest, RejectsMalformedVersionsOversizeAndInvalidSessionIds)
{
    for (const auto& bytes :
         {QByteArray{"{broken\n"}, QByteArray{"[]\n"}, QByteArray{"{\"type\":\"ready\",\"version\":2}\n"},
          QByteArray(msa::infra::VoiceProtocol::kMaximumFrameBytes + 1, 'x')})
    {
        QByteArray pending;
        EXPECT_THROW(static_cast<void>(msa::infra::VoiceProtocol::consume(pending, bytes)), std::runtime_error);
    }
    for (const auto* identifier : {"0", "01", "-1", "18446744073709551616"})
    {
        EXPECT_THROW(static_cast<void>(msa::infra::VoiceProtocol::session({{"session", identifier}})),
                     std::runtime_error);
    }
}
class SpeechRecognizerTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void configure(const QString& scenario);
    QTemporaryDir m_directory{};
    msa::infra::ProcessSpeechRecognizer m_recognizer{QString::fromUtf8(MSA_FAKE_VOICE_WORKER), {1500ms, 100ms, 300ms}};
    std::vector<std::string> m_texts{};
    std::vector<std::string> m_errors{};
    int m_recordings{};
};
void SpeechRecognizerTest::SetUp()
{
    ASSERT_TRUE(m_directory.isValid());
    m_recognizer.setHandler(
        [this](const msa::core::RecognitionEvent& event)
        {
            if (event.kind == Kind::Text)
            {
                m_texts.push_back(event.text);
            }
            if (event.kind == Kind::Error)
            {
                m_errors.push_back(event.text);
            }
            if (event.kind == Kind::State && event.state == State::Recording)
            {
                ++m_recordings;
            }
        });
}
void SpeechRecognizerTest::configure(const QString& scenario)
{
    m_recognizer.configure(m_directory.filePath(scenario).toStdString());
}
TEST_F(SpeechRecognizerTest, ConfigurationDoesNotLaunchOrRecordAndExplicitStartReusesModel)
{
    EXPECT_EQ(m_recognizer.state(), State::Unconfigured);
    configure("normal");
    settle();
    EXPECT_EQ(m_recognizer.state(), State::Idle);
    EXPECT_EQ(m_recordings, 0);
    m_recognizer.start();
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready && m_texts.size() == 1; }));
    EXPECT_EQ(m_texts.front(), "Распознанная фраза 🙂");
    EXPECT_EQ(m_recordings, 1);
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready && m_texts.size() == 2; }));
    EXPECT_EQ(m_recordings, 2);
    EXPECT_TRUE(m_errors.empty());
}
TEST_F(SpeechRecognizerTest, ManualFinishReturnsFinalTextAndRecordingDeadlineFinishesAutomatically)
{
    configure("finish");
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Recording; }));
    m_recognizer.finish();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready; }));
    EXPECT_EQ(m_texts.size(), 1);
    EXPECT_TRUE(m_errors.empty());
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready && m_texts.size() == 2; }));
    EXPECT_EQ(m_errors.size(), 1);
}
TEST_F(SpeechRecognizerTest, CancelDropsLateResultsAndIgnoresForeignSession)
{
    configure("late");
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Recording; }));
    m_recognizer.cancel();
    EXPECT_EQ(m_recognizer.state(), State::Idle);
    settle();
    EXPECT_TRUE(m_texts.empty());
    configure("wrong_session");
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready && m_texts.size() == 1; }));
    EXPECT_EQ(m_texts.front(), "Распознанная фраза 🙂");
}
TEST_F(SpeechRecognizerTest, BadFramesCrashAndTimeoutStopWithoutAutomaticRestart)
{
    for (const auto* scenario : {"bad_json", "oversize", "crash", "never_finish"})
    {
        configure(scenario);
        m_recognizer.start();
        ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Failed; })) << scenario;
        const auto recordings = m_recordings;
        settle();
        EXPECT_EQ(m_recordings, recordings);
        EXPECT_FALSE(m_errors.empty());
        EXPECT_EQ(m_recognizer.state(), State::Failed);
    }
}
TEST_F(SpeechRecognizerTest, MissingExecutableAndUnresponsiveModelDoNotBlockEventLoop)
{
    msa::infra::ProcessSpeechRecognizer missing{m_directory.filePath("missing.exe"), {100ms, 100ms, 100ms}};
    missing.configure(m_directory.path().toStdString());
    missing.start();
    ASSERT_TRUE(waitFor([&missing] { return missing.state() == State::Failed; }));
    msa::infra::ProcessSpeechRecognizer slow{QString::fromUtf8(MSA_FAKE_VOICE_WORKER), {200ms, 100ms, 100ms}};
    slow.configure(m_directory.filePath("loading_timeout").toStdString());
    bool eventHandled{false};
    QTimer::singleShot(0, &slow, [&eventHandled] { eventHandled = true; });
    slow.start();
    ASSERT_TRUE(waitFor([&slow] { return slow.state() == State::Failed; }));
    EXPECT_TRUE(eventHandled);
    slow.shutdown();
    EXPECT_EQ(slow.state(), State::Idle);
}
TEST_F(SpeechRecognizerTest, CancelFromStateCallbackDoesNotStartCancelledWorker)
{
    configure("normal");
    m_recognizer.setHandler(
        [this](const msa::core::RecognitionEvent& event)
        {
            if (event.kind == Kind::State && event.state == State::Loading)
            {
                m_recognizer.cancel();
            }
            if (event.kind == Kind::Text)
            {
                m_texts.push_back(event.text);
            }
        });
    m_recognizer.start();
    EXPECT_EQ(m_recognizer.state(), State::Idle);
    settle();
    EXPECT_EQ(m_recognizer.state(), State::Idle);
    EXPECT_TRUE(m_texts.empty());
}
TEST_F(SpeechRecognizerTest, FinishFromRecordingCallbackArrivesAfterStartCommand)
{
    configure("finish");
    m_recognizer.setHandler(
        [this](const msa::core::RecognitionEvent& event)
        {
            if (event.kind == Kind::State && event.state == State::Recording)
            {
                m_recognizer.finish();
            }
            if (event.kind == Kind::Text)
            {
                m_texts.push_back(event.text);
            }
        });
    m_recognizer.start();
    ASSERT_TRUE(waitFor([this] { return m_recognizer.state() == State::Ready && m_texts.size() == 1; }));
    EXPECT_EQ(m_texts.front(), "Распознанная фраза 🙂");
}
TEST_F(SpeechRecognizerTest, NativeVoskWorkerLoadsItsDllAndRejectsMissingModelWithoutOpeningMicrophone)
{
    msa::infra::ProcessSpeechRecognizer native{QString::fromUtf8(MSA_VOICE_WORKER), {1500ms, 100ms, 100ms}};
    QString error;
    native.setHandler(
        [&error](const msa::core::RecognitionEvent& event)
        {
            if (event.kind == Kind::Error)
            {
                error = QString::fromStdString(event.text);
            }
        });
    native.configure(m_directory.filePath("missing-model").toStdString());
    native.start();
    ASSERT_TRUE(waitFor([&native] { return native.state() == State::Failed; }));
    EXPECT_TRUE(error.contains(QString::fromUtf8("модель Vosk"))) << error.toStdString();
    native.shutdown();
}
} // namespace
