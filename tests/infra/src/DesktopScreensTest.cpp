#include "msa/ui/DesktopState.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QPointF>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>

#include <algorithm>
#include <optional>

namespace
{
constexpr int kDesktopWidth = 1200;
constexpr int kDesktopHeight = 900;
constexpr int kWaitMilliseconds = 3000;
constexpr int kLayoutSettleMilliseconds = 150;

class DesktopScreensTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QObject::connect(m_view.engine(), &QQmlEngine::warnings, &m_view,
                         [this](const QList<QQmlError>& errors)
                         {
                             for (const auto& error : errors)
                             {
                                 m_warnings.append(error.toString());
                             }
                         });
        m_view.setInitialProperties(
            {{"client", QVariant::fromValue(&m_models)}, {"desktop", QVariant::fromValue(&m_state)}});
        m_view.setResizeMode(QQuickView::SizeRootObjectToView);
        m_view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Desktop/DesktopShell.qml"});
        ASSERT_EQ(m_view.status(), QQuickView::Ready) << m_view.errors().value(0).toString().toStdString();
        m_view.resize(kDesktopWidth, kDesktopHeight);
        m_view.show();
        QElapsedTimer timer;
        timer.start();
        while (!m_view.isExposed() && timer.elapsed() < kWaitMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }
        ASSERT_TRUE(m_view.isExposed());
        QGuiApplication::processEvents();
    }
    void TearDown() override
    {
        EXPECT_TRUE(m_warnings.isEmpty()) << m_warnings.join('\n').toStdString();
    }
    [[nodiscard]] QQuickItem* item(const char* name)
    {
        QList<QQuickItem*> remaining{m_view.contentItem()};
        const auto requested = QString::fromLatin1(name);
        while (!remaining.isEmpty())
        {
            auto* current = remaining.takeLast();
            if (current->objectName() == requested)
            {
                return current;
            }
            remaining.append(current->childItems());
        }
        return nullptr;
    }
    void activate(QQuickItem* target)
    {
        ASSERT_NE(target, nullptr);
        target->forceActiveFocus(Qt::TabFocusReason);
        QKeyEvent press{QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier};
        QKeyEvent release{QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier};
        QGuiApplication::sendEvent(&m_view, &press);
        QGuiApplication::sendEvent(&m_view, &release);
        QGuiApplication::processEvents();
    }
    msa::ui::ClientModels m_models{};
    msa::ui::DesktopState m_state{m_models};
    QQuickView m_view{};
    QStringList m_warnings{};
};

TEST_F(DesktopScreensTest, NavigatesAllPagesWithoutServer)
{
    for (int page{0}; page < 4; ++page)
    {
        const auto name = QString{"navigation%1"}.arg(page).toLatin1();
        activate(item(name.constData()));
        EXPECT_EQ(m_state.page(), page);
    }
    m_state.navigate(-1);
    m_state.navigate(4);
    EXPECT_EQ(m_state.page(), 3);
    EXPECT_FALSE(item("sendMessageButton")->isEnabled());
}

TEST_F(DesktopScreensTest, SettingsMaskTokenAndDispatchValidatedPreferencesWhileOffline)
{
    m_state.navigate(3);
    item("settingsHost")->setProperty("text", "localhost");
    item("settingsPort")->setProperty("text", "43210");
    item("settingsToken")->setProperty("text", "private-test-token");
    EXPECT_EQ(item("settingsToken")->property("echoMode").toInt(), 2);
    EXPECT_TRUE(item("autoConnectCheck")->property("checked").toBool());
    activate(item("autoConnectCheck"));
    std::optional<msa::core::ClientPreferences> saved;
    QObject::connect(&m_state, &msa::ui::DesktopState::saveRequested, &m_view,
                     [&saved](const msa::core::ClientPreferences& value) { saved = value; });
    activate(item("saveSettingsButton"));
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->host, "localhost");
    EXPECT_EQ(saved->port, 43210);
    EXPECT_FALSE(saved->autoConnect);
    int connections{0};
    QObject::connect(&m_state, &msa::ui::DesktopState::connectRequested, &m_view, [&connections]() { ++connections; });
    EXPECT_FALSE(item("connectSettingsButton")->isEnabled());
    m_state.applyPreferences(*saved);
    EXPECT_TRUE(item("connectSettingsButton")->isEnabled());
    activate(item("connectSettingsButton"));
    EXPECT_EQ(connections, 1);
}

TEST_F(DesktopScreensTest, RendersPopulatedScreensAndRetainsRowsOffline)
{
    const auto date = std::chrono::year{2026} / 11 / 9;
    const auto now =
        std::chrono::local_seconds{std::chrono::sys_days{date}.time_since_epoch()} + std::chrono::hours{19};
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    snapshot.serverTime = now;
    snapshot.timeZone = "Asia/Novosibirsk";
    snapshot.tasks.push_back({17, "Прочитать главу", date, msa::core::TaskPriority::Normal, false, now});
    snapshot.habits.push_back({{18, "Чтение", 20, "стр.", msa::core::HabitPeriod::Day}, 12});
    snapshot.reminders.push_back(
        msa::core::TimedReminder{19, "Сделать перерыв", now, msa::core::ReminderStatus::Pending});
    snapshot.reminders.push_back(msa::core::RecurringReminder{20, "Размяться", now, std::chrono::minutes{60}});
    snapshot.reminders.push_back(
        msa::core::PlaceReminder{21, "Купить продукты", "Магазин", {}, msa::core::ReminderStatus::Pending});
    snapshot.eventsToday.push_back(
        {{22, "Время для себя", date, std::chrono::minutes{19 * 60}, "Отдых и чтение", msa::core::EventRepeat::Daily},
         date});
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    EXPECT_TRUE(m_models.tasks()
                    ->data(m_models.tasks()->index(0), m_models.tasks()->roleNames().key("detailsText"))
                    .toString()
                    .contains(QString::fromUtf8("Обычный приоритет")));
    ASSERT_TRUE(m_models.chats()->replaceRows({msa::core::Chat{1, "Основной"}}));
    ASSERT_TRUE(m_models.selectChat("1"));
    ASSERT_TRUE(m_models.messages()->replaceRows({msa::core::ChatMessage{
        1, 1, msa::core::MessageRole::Assistant, "Загруженное сообщение доступно без связи.", now}}));
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    m_state.applyPreferences({"localhost", 43210, "demo-token", false});
    for (int page{0}; page < 4; ++page)
    {
        m_state.navigate(page);
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < kLayoutSettleMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }
        const auto preview = m_view.grabWindow();
        ASSERT_FALSE(preview.isNull());
        EXPECT_TRUE(preview.save(QString{"desktop-screen-%1.png"}.arg(page)));
    }
    EXPECT_EQ(m_models.tasks()->count(), 1);
    EXPECT_EQ(m_models.messages()->count(), 1);
    EXPECT_EQ(m_models.reminders()->count(), 3);
    EXPECT_EQ(m_models.habits()->count(), 1);
}

TEST_F(DesktopScreensTest, PlannerDialogUsesRealInputAndWaitsForAcknowledgement)
{
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    m_state.navigate(1);
    activate(item("newTaskButton"));
    auto* editor = m_state.planner();
    ASSERT_TRUE(editor->active());
    activate(item("plannerSubmitButton"));
    EXPECT_FALSE(editor->errorText().isEmpty());
    auto* title = item("plannerField_title");
    ASSERT_NE(title, nullptr);
    title->forceActiveFocus(Qt::TabFocusReason);
    const auto text = QString::fromUtf8("Название 🙂");
    QKeyEvent input{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, text};
    QGuiApplication::sendEvent(&m_view, &input);
    std::optional<msa::core::ClientPlannerAction> action;
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&action](const msa::core::ClientAction& value)
                     { action = std::get<msa::core::ClientPlannerAction>(value); });
    activate(item("plannerSubmitButton"));
    ASSERT_TRUE(action);
    EXPECT_EQ(std::get<msa::core::ClientSaveTask>(*action).title, text.toStdString());
    EXPECT_TRUE(editor->pending());
    EXPECT_FALSE(item("plannerSubmitButton")->isEnabled());
    EXPECT_EQ(m_models.tasks()->count(), 0);
    EXPECT_TRUE(m_view.grabWindow().save("planner-task-preview.png"));
    m_models.setError("Проверка серверного отказа");
    m_models.finishPlannerAction(*action, false);
    EXPECT_TRUE(editor->active());
    EXPECT_EQ(item("plannerField_title")->property("text").toString(), text);
    activate(item("plannerSubmitButton"));
    m_models.finishPlannerAction(*action, true);
    EXPECT_FALSE(editor->active());
}

TEST_F(DesktopScreensTest, PlannerDialogChangesReminderFieldsAndCancelsDestructiveAction)
{
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    snapshot.tasks.push_back({17, "Не удалять без подтверждения", {}, msa::core::TaskPriority::Normal, false, {}});
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    m_state.navigate(2);
    activate(item("newReminderButton"));
    auto* editor = m_state.planner();
    auto* title = item("plannerField_title");
    ASSERT_NE(title, nullptr);
    title->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent input{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("Напоминание")};
    QGuiApplication::sendEvent(&m_view, &input);
    auto* kind = item("plannerChoice_kind");
    ASSERT_NE(kind, nullptr);
    kind->forceActiveFocus(Qt::TabFocusReason);
    for (int step{0}; step < 2; ++step)
    {
        QKeyEvent next{QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier};
        QKeyEvent release{QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier};
        QGuiApplication::sendEvent(&m_view, &next);
        QGuiApplication::sendEvent(&m_view, &release);
        QGuiApplication::processEvents();
        EXPECT_TRUE(kind->hasActiveFocus());
    }
    QGuiApplication::processEvents();
    EXPECT_EQ(item("plannerField_title")->property("text").toString(), QString::fromUtf8("Напоминание"));
    auto* place = item("plannerField_place");
    ASSERT_NE(place, nullptr);
    EXPECT_TRUE(place->isVisible());
    place->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent address{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("Магазин")};
    QGuiApplication::sendEvent(&m_view, &address);
    std::vector<msa::core::ClientPlannerAction> actions;
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&actions](const msa::core::ClientAction& value)
                     { actions.push_back(std::get<msa::core::ClientPlannerAction>(value)); });
    activate(item("plannerSubmitButton"));
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(std::get<msa::core::ClientCreatePlaceReminder>(actions.front()).place, "Магазин");
    EXPECT_TRUE(m_view.grabWindow().save("planner-reminder-preview.png"));
    m_models.finishPlannerAction(actions.front(), true);
    editor->deleteTask("17");
    QGuiApplication::processEvents();
    activate(item("plannerCancelButton"));
    EXPECT_EQ(actions.size(), 1U);
    EXPECT_EQ(m_models.tasks()->count(), 1);
}

TEST_F(DesktopScreensTest, CalendarRejectsInvalidDateWithoutReadRequest)

{
    int reads{0};
    QObject::connect(&m_models, &msa::ui::ClientModels::readRequested, &m_view,
                     [&reads](msa::core::ClientReadRequest, quint64, quint64) { ++reads; });
    EXPECT_FALSE(m_state.showCalendarDay("2026-02-30"));
    EXPECT_EQ(reads, 0);
    EXPECT_FALSE(m_models.errorText().isEmpty());
}

TEST_F(DesktopScreensTest, DispatchesExactChatAndTaskActionsOnlyWhileOnline)
{
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    snapshot.tasks.push_back({17, "Задача", {}, msa::core::TaskPriority::Normal, false, {}});
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    ASSERT_TRUE(m_models.chats()->replaceRows({msa::core::Chat{1, "Основной"}}));
    ASSERT_TRUE(m_models.selectChat("1"));
    std::vector<msa::core::ClientAction> actions;
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&actions](const msa::core::ClientAction& action) { actions.push_back(action); });
    item("chatComposer")->setProperty("text", QString::fromUtf8("Текст <b>🙂</b>\nвторая строка"));
    item("chatSearchQuery")->setProperty("text", "C++ & Qt");
    activate(item("sendMessageButton"));
    ASSERT_EQ(actions.size(), 1U);
    const auto* message = std::get_if<msa::core::ClientSendMessage>(&actions.front());
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->text, "Текст <b>🙂</b>\nвторая строка");
    EXPECT_EQ(message->searchQuery, "C++ & Qt");
    EXPECT_FALSE(item("chatComposer")->property("text").toString().isEmpty());
    m_state.navigate(1);
    QGuiApplication::processEvents();
    activate(item("taskCheckBox"));
    ASSERT_EQ(actions.size(), 2U);
    const auto* task = std::get_if<msa::core::ClientSetTaskDone>(&actions.back());
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->taskIdentifier, 17);
    EXPECT_TRUE(task->done);
    EXPECT_FALSE(item("taskCheckBox")->property("checked").toBool());
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    EXPECT_FALSE(item("taskCheckBox")->isEnabled());
    EXPECT_FALSE(item("sendMessageButton")->isEnabled());
    EXPECT_EQ(actions.size(), 2U);
}
TEST_F(DesktopScreensTest, MusicRemainsUsableAcrossAllPagesAndDisconnectedSessions)
{
    msa::core::MusicState state;
    state.folder = "C:/Music";
    state.title = "Вечерняя музыка <b>🙂</b>";
    state.position = 2;
    state.count = 7;
    state.volume = 0.42;
    state.playing = true;
    auto* music = m_state.music();
    std::vector<msa::core::MusicCommand> commands;
    QObject::connect(music, &msa::ui::MusicModel::commandRequested, &m_view,
                     [&commands](const msa::core::MusicCommand& command) { commands.push_back(command); });
    music->applyState(state);
    EXPECT_TRUE(commands.empty());
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    for (int page{0}; page < 4; ++page)
    {
        m_state.navigate(page);
        QGuiApplication::processEvents();
        ASSERT_NE(item("musicPanel"), nullptr);
        EXPECT_TRUE(item("musicPanel")->isVisible());
        EXPECT_TRUE(item("musicNextButton")->isEnabled());
        activate(item("musicNextButton"));
        EXPECT_EQ(commands.back().kind, msa::core::MusicCommandKind::Next);
    }
    activate(item("musicPlayButton"));
    EXPECT_EQ(commands.back().kind, msa::core::MusicCommandKind::PlayPause);
    activate(item("musicPreviousButton"));
    EXPECT_EQ(commands.back().kind, msa::core::MusicCommandKind::Previous);
    activate(item("musicShuffleButton"));
    EXPECT_EQ(commands.back().kind, msa::core::MusicCommandKind::Reshuffle);
    auto* slider = item("musicVolumeSlider");
    ASSERT_NE(slider, nullptr);
    EXPECT_EQ(slider->property("value").toDouble(), 42);
    slider->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent press{QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier};
    QKeyEvent release{QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier};
    QGuiApplication::sendEvent(&m_view, &press);
    QGuiApplication::sendEvent(&m_view, &release);
    EXPECT_EQ(commands.back().kind, msa::core::MusicCommandKind::SetVolume);
    EXPECT_DOUBLE_EQ(commands.back().volume, 0.43);
    EXPECT_TRUE(music->playing());
    EXPECT_EQ(music->positionText(), "2 / 7");
    m_state.navigate(0);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    const auto image = m_view.grabWindow();
    ASSERT_FALSE(image.isNull());
    ASSERT_TRUE(image.save("music-panel-preview.png"));
    state.scanning = true;
    music->applyState(state);
    EXPECT_FALSE(item("musicNextButton")->isEnabled());
    EXPECT_TRUE(item("musicFolderButton")->isEnabled());
    state.scanning = false;
    state.count = 0;
    state.position = 0;
    state.playing = false;
    state.error = "В папке нет поддерживаемых аудиофайлов.";
    music->applyState(state);
    EXPECT_FALSE(item("musicPlayButton")->isEnabled());
    EXPECT_FALSE(music->errorText().isEmpty());
    EXPECT_TRUE(m_models.errorText().isEmpty());
}
TEST_F(DesktopScreensTest, AudioDeviceControlsChooseIdsOnlyOnUserActivationAndRefreshOffline)
{
    auto* devices = m_state.audioDevices();
    msa::core::AudioDevicesState state;
    state.inputs = {{"", "По умолчанию", true}, {"YQ==", "Микрофон гарнитуры", true}};
    state.outputs = {{"", "По умолчанию", true}, {"Yg==", "Наушники", true}};
    std::vector<QString> selections;
    int refreshes{};
    QObject::connect(devices, &msa::ui::AudioDevicesModel::selectionRequested, &m_view,
                     [&selections](bool, const QString& identifier) { selections.push_back(identifier); });
    QObject::connect(devices, &msa::ui::AudioDevicesModel::refreshRequested, &m_view, [&refreshes] { ++refreshes; });
    devices->applyState(state);
    m_state.navigate(3);
    QGuiApplication::processEvents();
    ASSERT_TRUE(selections.empty());
    auto* input = item("inputDeviceCombo");
    ASSERT_NE(input, nullptr);
    input->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent press{QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier};
    QKeyEvent release{QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier};
    QGuiApplication::sendEvent(&m_view, &press);
    QGuiApplication::sendEvent(&m_view, &release);
    ASSERT_FALSE(selections.empty());
    EXPECT_EQ(selections.back(), "YQ==");
    auto* output = item("outputDeviceCombo");
    ASSERT_NE(output, nullptr);
    output->forceActiveFocus(Qt::TabFocusReason);
    QGuiApplication::sendEvent(&m_view, &press);
    QGuiApplication::sendEvent(&m_view, &release);
    EXPECT_EQ(selections.back(), "Yg==");
    activate(item("refreshAudioDevicesButton"));
    EXPECT_EQ(refreshes, 1);
    const auto count = selections.size();
    state.preferences = {"YQ==", "Yg=="};
    devices->applyState(state);
    EXPECT_EQ(selections.size(), count);
    auto* scroll = item("settingsScroll");
    ASSERT_NE(scroll, nullptr);
    auto* flickable = scroll->property("contentItem").value<QQuickItem*>();
    ASSERT_NE(flickable, nullptr);
    const auto maximum = std::max(0.0, flickable->property("contentHeight").toDouble() - flickable->height());
    const auto position = std::clamp(input->mapToItem(flickable, QPointF{}).y(), 0.0, maximum);
    ASSERT_TRUE(flickable->setProperty("contentY", position));
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    EXPECT_TRUE(m_view.grabWindow().save("audio-devices-preview.png"));
}
TEST_F(DesktopScreensTest, MessageSpeechIsExplicitWorksOfflineAndCanBeStopped)
{
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    snapshot.timeZone = "Etc/UTC";
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    ASSERT_TRUE(m_models.chats()->replaceRows({msa::core::Chat{1, "Основной"}}));
    ASSERT_TRUE(m_models.selectChat("1"));
    ASSERT_TRUE(m_models.messages()->replaceRows(
        {msa::core::ChatMessage{1, 1, msa::core::MessageRole::Assistant, "Ответ <b>🙂</b>", {}}}));
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    QString spoken;
    int stops{};
    int serverActions{};
    auto* voice = m_state.voice();
    QObject::connect(voice, &msa::ui::VoiceModel::speechRequested, &m_view,
                     [&spoken](const QString& text) { spoken = text; });
    QObject::connect(voice, &msa::ui::VoiceModel::stopSpeechRequested, &m_view, [&stops] { ++stops; });
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&serverActions](const msa::core::ClientAction&) { ++serverActions; });
    QElapsedTimer timer;
    timer.start();
    while (!item("speakMessageButton") && timer.elapsed() < kWaitMilliseconds)
    {
        QGuiApplication::processEvents();
    }
    ASSERT_NE(item("speakMessageButton"), nullptr);
    EXPECT_TRUE(spoken.isEmpty());
    activate(item("speakMessageButton"));
    EXPECT_EQ(spoken, QString::fromUtf8("Ответ <b>🙂</b>"));
    voice->applySpeechEvent({msa::core::SpeechState::Speaking, {}});
    QGuiApplication::processEvents();
    ASSERT_TRUE(item("stopSpeechButton")->isVisible());
    activate(item("stopSpeechButton"));
    EXPECT_EQ(stops, 1);
    EXPECT_EQ(serverActions, 0);
    m_view.resize(1000, 720);
    timer.restart();
    while (timer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    EXPECT_TRUE(m_view.grabWindow().save("speech-output-preview.png"));
    constexpr int kMinimumMessageHeight = 88;
    EXPECT_GE(item("messageList")->height(), kMinimumMessageHeight);
    const auto* send = item("sendMessageButton");
    const auto* panel = item("musicPanel");
    EXPECT_LE(send->mapToScene(QPointF{0, send->height()}).y(), panel->mapToScene(QPointF{}).y());
    const auto* button = item("stopSpeechButton");
    EXPECT_LE(button->mapToScene(QPointF{button->width(), button->height()}).x(), m_view.width());
    voice->applySpeechEvent({msa::core::SpeechState::Idle, {}});
    EXPECT_FALSE(item("stopSpeechButton")->isVisible());
}
TEST_F(DesktopScreensTest, DictationChangesDraftOfflineAndNeverSendsServerAction)
{
    auto* voice = m_state.voice();
    voice->applyModelPath("C:/Models/Russian");
    std::vector<msa::core::RecognitionCommand> commands;
    QObject::connect(voice, &msa::ui::VoiceModel::commandRequested, &m_view,
                     [&commands](msa::core::RecognitionCommand command) { commands.push_back(command); });
    voice->setDraft("Проверить список");
    EXPECT_TRUE(item("chatComposer")->isEnabled());
    QGuiApplication::processEvents();
    EXPECT_EQ(item("chatComposer")->property("text").toString(), QString::fromUtf8("Проверить список"));
    activate(item("voiceStartButton"));
    ASSERT_EQ(commands.size(), 1);
    EXPECT_EQ(commands.back(), msa::core::RecognitionCommand::Start);
    voice->applyEvent({msa::core::RecognitionEvent::Kind::State, msa::core::RecognitionState::Recording, {}});
    voice->applyEvent({msa::core::RecognitionEvent::Kind::Level, msa::core::RecognitionState::Recording, {}, 0.6});
    QElapsedTimer levelTimer;
    levelTimer.start();
    while (levelTimer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    ASSERT_NE(item("microphoneLevel"), nullptr);
    EXPECT_TRUE(item("microphoneLevel")->isVisible());
    EXPECT_DOUBLE_EQ(item("microphoneLevel")->property("value").toDouble(), 0.6);
    EXPECT_TRUE(m_view.grabWindow().save("microphone-level-preview.png"));
    activate(item("voiceFinishButton"));
    EXPECT_EQ(commands.back(), msa::core::RecognitionCommand::Finish);
    voice->applyEvent(
        {msa::core::RecognitionEvent::Kind::Text, msa::core::RecognitionState::Finishing, "Добавить встречу 🙂"});
    voice->applyEvent({msa::core::RecognitionEvent::Kind::State, msa::core::RecognitionState::Ready, {}});
    QGuiApplication::processEvents();
    EXPECT_EQ(item("chatComposer")->property("text").toString(),
              QString::fromUtf8("Проверить список\nДобавить встречу 🙂"));
    EXPECT_FALSE(item("sendMessageButton")->isEnabled());
    m_state.navigate(3);
    m_state.navigate(0);
    EXPECT_EQ(voice->draft(), QString::fromUtf8("Проверить список\nДобавить встречу 🙂"));
    voice->applyEvent(
        {msa::core::RecognitionEvent::Kind::Error, msa::core::RecognitionState::Failed, "Микрофон недоступен"});
    EXPECT_FALSE(voice->errorText().isEmpty());
    EXPECT_TRUE(m_models.errorText().isEmpty());
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    EXPECT_TRUE(m_view.grabWindow().save("voice-input-preview.png"));
    constexpr int kMinimumWidth = 1000;
    constexpr int kMinimumHeight = 720;
    constexpr int kMinimumEditorHeight = 44;
    m_view.resize(kMinimumWidth, kMinimumHeight);
    timer.restart();
    while (timer.elapsed() < kLayoutSettleMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    EXPECT_TRUE(m_view.grabWindow().save("voice-input-minimum-preview.png"));
    const auto* send = item("sendMessageButton");
    const auto* panel = item("musicPanel");
    EXPECT_LE(send->mapToScene(QPointF{0, send->height()}).y(), panel->mapToScene(QPointF{}).y());
    EXPECT_GE(item("chatComposer")->height(), kMinimumEditorHeight);
    msa::core::ClientSnapshot snapshot;
    snapshot.mainChatIdentifier = 1;
    snapshot.timeZone = "Etc/UTC";
    m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
    ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
    ASSERT_TRUE(m_models.chats()->replaceRows({msa::core::Chat{1, "Основной"}}));
    ASSERT_TRUE(m_models.selectChat("1"));
    int serverActions{};
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&serverActions](const msa::core::ClientAction&) { ++serverActions; });
    voice->applyEvent({msa::core::RecognitionEvent::Kind::State, msa::core::RecognitionState::Ready, {}});
    voice->start();
    voice->applyEvent(
        {msa::core::RecognitionEvent::Kind::Text, msa::core::RecognitionState::Ready, "Текст при работающем сервере"});
    QGuiApplication::processEvents();
    EXPECT_TRUE(item("sendMessageButton")->isEnabled());
    EXPECT_EQ(serverActions, 0);
}
} // namespace
