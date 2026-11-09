#include "msa/ui/DesktopState.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>
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
        QList<QQuickItem*> remaining{m_view.rootObject()};
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
} // namespace
