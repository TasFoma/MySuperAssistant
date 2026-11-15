#include "msa/ui/MobileState.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickView>
#include <QRectF>
#include <QThread>
#include <optional>

namespace
{
constexpr int kWaitMilliseconds = 3000;
constexpr int kSettleMilliseconds = 120;
constexpr int kPhoneWidth = 390;
constexpr int kPhoneHeight = 844;
constexpr int kNarrowWidth = 320;
constexpr int kKeyboardHeight = 420;
constexpr int kControlHeight = 44;
class MobileScreensTest : public ::testing::Test
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
            {{"client", QVariant::fromValue(&m_models)}, {"mobile", QVariant::fromValue(&m_state)}});
        m_view.setResizeMode(QQuickView::SizeRootObjectToView);
        m_view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
        ASSERT_EQ(m_view.status(), QQuickView::Ready) << m_view.errors().value(0).toString().toStdString();
        m_view.resize(kPhoneWidth, kPhoneHeight);
        m_view.show();
        QElapsedTimer timer;
        timer.start();
        while (!m_view.isExposed() && timer.elapsed() < kWaitMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }
        ASSERT_TRUE(m_view.isExposed());
        settle();
    }
    void TearDown() override
    {
        EXPECT_TRUE(m_warnings.isEmpty()) << m_warnings.join('\n').toStdString();
    }
    void settle()
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < kSettleMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }
    }
    [[nodiscard]] QQuickItem* item(const QString& name)
    {
        QList<QQuickItem*> remaining{m_view.contentItem()};
        while (!remaining.isEmpty())
        {
            auto* current = remaining.takeLast();
            if (current->objectName() == name)
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
        settle();
    }
    void populate()
    {
        const auto date = std::chrono::year{2024} / 2 / 29;
        const auto now = std::chrono::local_seconds{std::chrono::sys_days{date}.time_since_epoch()};
        msa::core::ClientSnapshot snapshot;
        snapshot.mainChatIdentifier = 1;
        snapshot.serverTime = now;
        snapshot.timeZone = "Asia/Novosibirsk";
        snapshot.tasks.push_back({17, "Прочитать главу", date, msa::core::TaskPriority::Normal, false, now});
        snapshot.habits.push_back({{18, "Чтение", 20, "стр.", msa::core::HabitPeriod::Day}, 12});
        snapshot.reminders.push_back(
            msa::core::TimedReminder{19, "Сделать перерыв", now, msa::core::ReminderStatus::Pending});
        m_models.setConnectionState(msa::core::ClientConnectionState::Ready);
        ASSERT_TRUE(m_models.applyResult(snapshot, m_models.sessionRevision(), m_models.viewRevision()));
        ASSERT_TRUE(m_models.chats()->replaceRows({msa::core::Chat{1, "Основной"}, msa::core::Chat{2, "Книги"}}));
        ASSERT_TRUE(m_state.selectConversation("1"));
        ASSERT_TRUE(m_models.messages()->replaceRows(
            {msa::core::ChatMessage{1, 1, msa::core::MessageRole::Assistant,
                                    "Сообщения, планы и напоминания — на одном экране телефона.", now}}));
        settle();
    }
    msa::ui::ClientModels m_models{};
    msa::ui::MobileState m_state{m_models};
    QQuickView m_view{};
    QStringList m_warnings{};
};
TEST_F(MobileScreensTest, NavigationAndSeparateConversationPickerPreserveOfflineDraft)
{
    populate();
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    auto* composer = item("mobileComposer");
    ASSERT_NE(composer, nullptr);
    composer->forceActiveFocus();
    QKeyEvent input{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("Черновик 🙂")};
    QGuiApplication::sendEvent(&m_view, &input);
    EXPECT_EQ(m_state.draft(), QString::fromUtf8("Черновик 🙂"));
    for (int page{0}; page < 4; ++page)
    {
        activate(item(QString{"mobileNavigation%1"}.arg(page)));
        EXPECT_EQ(m_state.page(), page);
    }
    m_state.navigate(0);
    activate(item("mobileConversationsButton"));
    EXPECT_TRUE(m_state.conversationsVisible());
    ASSERT_TRUE(m_state.selectConversation("2"));
    EXPECT_FALSE(m_state.conversationsVisible());
    EXPECT_EQ(m_state.draft(), QString::fromUtf8("Черновик 🙂"));
    EXPECT_FALSE(item("mobileSendButton")->isEnabled());
    EXPECT_TRUE(m_state.goBack() == false);
    m_state.navigate(-1);
    m_state.navigate(4);
    EXPECT_EQ(m_state.page(), 0);
}
TEST_F(MobileScreensTest, SharedSettingsValidateMaskTokenAndRequireSavedParametersToConnect)
{
    m_state.navigate(3);
    item("mobileSettingsHost")->setProperty("text", "localhost");
    item("mobileSettingsPort")->setProperty("text", "65536");
    item("mobileSettingsToken")->setProperty("text", "private-test-token");
    EXPECT_EQ(item("mobileSettingsToken")->property("echoMode").toInt(), 2);
    std::optional<msa::core::ClientPreferences> saved;
    auto* settings = m_state.settings();
    QObject::connect(settings, &msa::ui::ClientSettingsModel::saveRequested, &m_view,
                     [&saved](msa::core::ClientPreferences preferences) { saved = preferences; });
    activate(item("mobileSaveSettings"));
    EXPECT_FALSE(saved);
    EXPECT_FALSE(settings->settingsMessage().contains("private-test-token"));
    item("mobileSettingsPort")->setProperty("text", "43210");
    activate(item("mobileSaveSettings"));
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->host, "localhost");
    EXPECT_FALSE(item("mobileConnectSettings")->isEnabled());
    settings->applyPreferences(*saved);
    int connects{0};
    QObject::connect(settings, &msa::ui::ClientSettingsModel::connectRequested, &m_view, [&connects]() { ++connects; });
    activate(item("mobileConnectSettings"));
    EXPECT_EQ(connects, 1);
}
TEST_F(MobileScreensTest, CalendarUsesServerDateLeapDayAndRejectsLateMonthResults)
{
    populate();
    EXPECT_EQ(m_state.selectedDate(), "2024-02-29");
    auto cells = m_state.calendarCells();
    ASSERT_EQ(cells.size(), 35);
    EXPECT_FALSE(cells.front().toMap()["valid"].toBool());
    EXPECT_EQ(cells[3].toMap()["date"].toString(), "2024-02-01");
    EXPECT_TRUE(m_models.applyResult(msa::core::ClientCalendarMonth{std::chrono::year{2024} / 2, {29}},
                                     m_models.sessionRevision(), m_models.viewRevision()));
    EXPECT_TRUE(m_state.calendarCells()[31].toMap()["hasEvents"].toBool());
    m_state.shiftMonth(1);
    EXPECT_EQ(m_state.selectedDate(), "2024-03-01");
    EXPECT_FALSE(m_models.applyResult(msa::core::ClientCalendarMonth{std::chrono::year{2024} / 2, {29}},
                                      m_models.sessionRevision(), m_models.viewRevision()));
    for (const auto& cell : m_state.calendarCells())
    {
        EXPECT_FALSE(cell.toMap()["hasEvents"].toBool());
    }
    EXPECT_FALSE(m_state.selectDay("2024-02-29"));
    EXPECT_FALSE(m_state.selectDay("2024-03-32"));
    m_models.invalidateSession();
    m_state.shiftMonth(-1);
    EXPECT_EQ(m_state.selectedDate(), "2024-02-01");
}
TEST_F(MobileScreensTest, SharedPlannerWaitsForAcknowledgementAndBackDoesNotDelete)
{
    m_view.resize(kNarrowWidth, kPhoneHeight);
    populate();
    m_state.navigate(1);
    activate(item("mobileNewTask"));
    auto* editor = m_state.planner();
    ASSERT_TRUE(editor->active());
    editor->setField("title", "Новая задача");
    settle();
    EXPECT_TRUE(m_view.grabWindow().save("mobile-planner-preview.png"));
    std::optional<msa::core::ClientPlannerAction> sent;
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&sent](const msa::core::ClientAction& action)
                     {
                         if (const auto* planner = std::get_if<msa::core::ClientPlannerAction>(&action))
                         {
                             sent = *planner;
                         }
                     });
    activate(item("plannerSubmitButton"));
    ASSERT_TRUE(sent);
    EXPECT_TRUE(editor->pending());
    EXPECT_EQ(m_models.tasks()->count(), 1);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_TRUE(editor->active());
    m_models.finishPlannerAction(*sent, true);
    EXPECT_FALSE(editor->active());
    sent.reset();
    editor->deleteTask("17");
    EXPECT_TRUE(m_state.goBack());
    EXPECT_FALSE(sent);
    EXPECT_EQ(m_models.tasks()->count(), 1);
}
TEST_F(MobileScreensTest, ExplicitSendPreservesDraftAndSearchChoiceForRetry)
{
    populate();
    m_state.setDraft("Вопрос с поиском");
    m_state.setSearchQuery("исходная фраза");
    std::optional<msa::core::ClientAction> sent;
    QObject::connect(&m_models, &msa::ui::ClientModels::actionRequested, &m_view,
                     [&sent](msa::core::ClientAction action) { sent = std::move(action); });
    activate(item("mobileSendButton"));
    ASSERT_TRUE(sent);
    EXPECT_EQ(std::get<msa::core::ClientSendMessage>(*sent).text, "Вопрос с поиском");
    EXPECT_EQ(m_state.draft(), QString::fromUtf8("Вопрос с поиском"));
    m_models.setError("Ошибка модели", "user_saved", "17");
    m_state.setSearchQuery("другая фраза");
    ASSERT_TRUE(m_state.retryMessage());
    EXPECT_EQ(std::get<msa::core::ClientRetryMessage>(*sent).searchQuery.value(), "исходная фраза");
    m_models.setError("Результат неизвестен", "unknown");
    EXPECT_FALSE(m_state.retryMessage());
    m_models.resetServerData();
    populate();
    m_models.setError("Ошибка другого сервера", "user_saved", "17");
    EXPECT_FALSE(m_state.retryMessage());
    EXPECT_EQ(m_state.draft(), QString::fromUtf8("Вопрос с поиском"));
}
TEST_F(MobileScreensTest, NarrowPagesRenderWithoutHorizontalOverflowAndRetainOfflineData)
{
    populate();
    m_state.settings()->applyPreferences({"localhost", 43210, "preview-token", false});
    m_models.invalidateSession();
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    m_view.resize(kNarrowWidth, kPhoneHeight);
    for (int page{0}; page < 4; ++page)
    {
        m_state.navigate(page);
        settle();
        auto* banner = item("mobileConnectionBanner");
        ASSERT_NE(banner, nullptr);
        EXPECT_GE(banner->height(), kControlHeight);
        const auto bannerBounds = banner->mapRectToScene(QRectF{0, 0, banner->width(), banner->height()});
        auto* area = item("mobilePageArea");
        ASSERT_NE(area, nullptr);
        const auto areaBounds = area->mapRectToScene(QRectF{0, 0, area->width(), area->height()});
        EXPECT_GE(bannerBounds.top(), 0) << "page=" << page;
        EXPECT_GE(areaBounds.top(), bannerBounds.bottom()) << "page=" << page;
        if (page == 1 || page == 2)
        {
            auto* screen = item(page == 1 ? "mobilePlansScroll" : "mobileRemindersScroll");
            ASSERT_NE(screen, nullptr);
            const auto screenBounds = screen->mapRectToScene(QRectF{0, 0, screen->width(), screen->height()});
            EXPECT_GE(screenBounds.top(), areaBounds.top()) << "page=" << page;
            EXPECT_LE(screenBounds.bottom(), areaBounds.bottom()) << "page=" << page;
            QList<QQuickItem*> descendants{screen};
            bool headingFound{false};
            while (!descendants.isEmpty())
            {
                auto* descendant = descendants.takeLast();
                if (descendant->property("text").toString() == (page == 1 ? "Планы" : "Напоминания"))
                {
                    headingFound = true;
                    EXPECT_GE(descendant->mapToScene(QPointF{}).y(), screenBounds.top()) << "page=" << page;
                }
                descendants.append(descendant->childItems());
            }
            EXPECT_TRUE(headingFound);
        }
        for (int index{0}; index < 4; ++index)
        {
            auto* button = item(QString{"mobileNavigation%1"}.arg(index));
            ASSERT_NE(button, nullptr);
            const auto bounds = button->mapRectToScene(button->boundingRect());
            EXPECT_GE(bounds.left(), 0);
            EXPECT_LE(bounds.right(), kNarrowWidth);
            EXPECT_GT(bounds.height(), 0);
        }
        // Снимаем компонент, а не содержимое оконного backing store программного backend.
        const auto capture = m_view.rootObject()->grabToImage();
        ASSERT_FALSE(capture.isNull());
        bool captured{false};
        QObject::connect(capture.data(), &QQuickItemGrabResult::ready, &m_view, [&captured]() { captured = true; });
        QElapsedTimer captureTimer;
        captureTimer.start();
        while (!captured && captureTimer.elapsed() < kWaitMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }
        ASSERT_TRUE(captured);
        const auto preview = capture->image();
        ASSERT_FALSE(preview.isNull());
        EXPECT_TRUE(preview.save(QString{"mobile-screen-%1.png"}.arg(page)));
    }
    EXPECT_EQ(m_models.tasks()->count(), 1);
    EXPECT_EQ(m_models.messages()->count(), 1);
}
TEST_F(MobileScreensTest, KeyboardSizedViewportKeepsComposerSendAndMessageAreaVisible)
{
    populate();
    m_view.resize(kNarrowWidth, kKeyboardHeight);
    m_view.rootObject()->setProperty("keyboardVisible", true);
    settle();
    for (const auto* name : {"mobileComposer", "mobileSendButton", "mobileMessageList"})
    {
        auto* target = item(QString::fromLatin1(name));
        ASSERT_NE(target, nullptr);
        SCOPED_TRACE(name);
        const auto bounds = target->mapRectToScene(QRectF{0, 0, target->width(), target->height()});
        EXPECT_GE(bounds.top(), 0);
        EXPECT_LE(bounds.bottom(), kKeyboardHeight);
        EXPECT_GE(bounds.height(), kControlHeight);
    }
    EXPECT_TRUE(m_view.grabWindow().save("mobile-keyboard-preview.png"));
}
} // namespace
