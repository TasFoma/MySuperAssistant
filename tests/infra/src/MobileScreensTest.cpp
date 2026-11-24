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
#include <algorithm>
#include <optional>

namespace
{
using MobilePage = msa::ui::MobileState::Page;
using MoreSection = msa::ui::MobileState::MoreSection;
using PlanSection = msa::ui::MobileState::PlanSection;

constexpr int kWaitMilliseconds = 3000;
// Ждём переход цвета 140 мс и следующий кадр программного рендеринга.
constexpr int kSettleMilliseconds = 220;
constexpr int kPhoneWidth = 390;
constexpr int kPhoneHeight = 844;
constexpr int kNarrowWidth = 320;
constexpr int kKeyboardHeight = 420;
constexpr int kControlHeight = 48;
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
        QQuickItem* fallback{};
        while (!remaining.isEmpty())
        {
            auto* current = remaining.takeLast();
            if (current->objectName() == name)
            {
                if (current->isVisible())
                {
                    return current;
                }
                fallback = current;
            }
            remaining.append(current->childItems());
        }

        return fallback;
    }
    [[nodiscard]] bool saveRender(const QString& path)
    {
        const auto capture = m_view.contentItem()->grabToImage();
        if (capture.isNull())
        {
            return false;
        }
        bool captured{};
        QObject::connect(capture.data(), &QQuickItemGrabResult::ready, &m_view, [&captured] { captured = true; });
        QElapsedTimer timer;
        timer.start();
        while (!captured && timer.elapsed() < kWaitMilliseconds)
        {
            QGuiApplication::processEvents();
            QThread::msleep(1);
        }

        return captured && capture->image().save(path);
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
        m_state.showMore(MoreSection::Assistant);
        settle();
    }
    msa::ui::ClientModels m_models{};
    msa::ui::MobileState m_state{m_models};
    QQuickView m_view{};
    QStringList m_warnings{};
};
TEST_F(MobileScreensTest, StartsTodayAndBackFollowsMoreHierarchy)
{
    EXPECT_EQ(m_state.page(), MobilePage::Today);
    EXPECT_FALSE(m_state.todayDate().isEmpty());
    activate(item("mobileOpenPlans"));
    EXPECT_EQ(m_state.page(), MobilePage::Plans);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_EQ(m_state.page(), MobilePage::Today);
    activate(item("mobileNavigation_more"));
    activate(item("mobileOpenAssistant"));
    EXPECT_EQ(m_state.moreSection(), MoreSection::Assistant);
    activate(item("mobileConversationsButton"));
    EXPECT_TRUE(m_state.conversationsVisible());
    EXPECT_TRUE(m_state.goBack());
    EXPECT_FALSE(m_state.conversationsVisible());
    EXPECT_EQ(m_state.moreSection(), MoreSection::Assistant);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_EQ(m_state.moreSection(), MoreSection::Menu);
    activate(item("mobileOpenSettings"));
    EXPECT_EQ(m_state.moreSection(), MoreSection::Settings);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_EQ(m_state.moreSection(), MoreSection::Menu);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_FALSE(m_state.goBack());
}

TEST_F(MobileScreensTest, DirtyFormBackPreservesInputUntilExplicitDiscard)
{
    populate();
    m_state.navigate(MobilePage::Plans);
    m_view.resize(360, kPhoneHeight);
    activate(item("mobileNewTask"));
    auto* field = item("plannerField_title");
    ASSERT_NE(field, nullptr);
    field->forceActiveFocus(Qt::TabFocusReason);
    const auto draft = QString::fromUtf8("Не потерять ввод 🙂");
    QKeyEvent input{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, draft};
    QGuiApplication::sendEvent(&m_view, &input);
    ASSERT_TRUE(m_state.planner()->dirty());
    EXPECT_TRUE(m_state.goBack());
    settle();
    EXPECT_TRUE(m_state.planner()->active());
    EXPECT_TRUE(m_state.planner()->discardRequested());
    EXPECT_TRUE(saveRender("mobile-discard-360-preview.png"));
    activate(item("keepEditingButton"));
    EXPECT_EQ(field->property("text").toString(), draft);
    EXPECT_FALSE(m_state.planner()->discardRequested());
    EXPECT_TRUE(m_state.goBack());
    activate(item("discardChangesButton"));
    EXPECT_FALSE(m_state.planner()->active());
    EXPECT_EQ(m_models.tasks()->count(), 1);
}

TEST_F(MobileScreensTest, InsetsAndKeyboardKeepNavigationAndEditorInsideViewport)
{
    populate();
    auto* shell = m_view.rootObject();
    shell->setProperty("safeTop", 24);
    shell->setProperty("safeBottom", 28);
    shell->setProperty("safeLeft", 4);
    shell->setProperty("safeRight", 4);
    for (const auto width : {360, 412})
    {
        m_view.resize(width, kPhoneHeight);
        shell->setProperty("keyboardVisible", false);
        settle();
        const auto* navigation = item("mobileBottomNavigation");
        const auto bottom = navigation->mapToScene(QPointF{0, navigation->height()}).y();
        EXPECT_LE(bottom, kPhoneHeight - 28);
        EXPECT_GE(item("mobilePageArea")->mapToScene(QPointF{}).y(), 24);
        m_view.resize(width, kKeyboardHeight);
        shell->setProperty("keyboardVisible", true);
        settle();
        EXPECT_FALSE(navigation->isVisible());
        for (const auto* name : {"mobileComposer", "mobileSendButton", "mobileMessageList"})
        {
            const auto* control = item(QString::fromLatin1(name));
            ASSERT_NE(control, nullptr);
            const auto bounds = control->mapRectToScene(QRectF{0, 0, control->width(), control->height()});
            EXPECT_GE(bounds.top(), 24) << name;
            EXPECT_LE(bounds.bottom(), kKeyboardHeight - 28) << name;
            EXPECT_GE(bounds.height(), 48) << name;
        }
        EXPECT_TRUE(saveRender(QStringLiteral("mobile-insets-keyboard-%1-preview.png").arg(width)));
    }
}

TEST_F(MobileScreensTest, PlannerTabsReuseFormsAndRenderAtPhoneWidths)
{
    populate();
    m_state.navigate(MobilePage::Plans);
    for (const auto width : {360, 412})
    {
        m_view.resize(width, kPhoneHeight);
        for (const auto& entry : m_state.planSections())
        {
            const auto tab = entry.toMap();
            m_state.selectPlanSection(tab.value("section").value<PlanSection>());
            settle();
            EXPECT_TRUE(item("mobileBottomNavigation")->isVisible());
            EXPECT_TRUE(saveRender(
                QStringLiteral("mobile-plans-%1-%2-preview.png").arg(tab.value("key").toString()).arg(width)));
            const auto key = tab.value("key").toString();
            const auto button = key == "tasks"      ? "mobileNewTask"
                                : key == "calendar" ? "mobileNewEvent"
                                : key == "habits"   ? "mobileNewHabit"
                                                    : "mobileNewReminder";
            activate(item(QString::fromLatin1(button)));
            EXPECT_TRUE(m_state.planner()->active());
            activate(item("plannerCancelButton"));
            EXPECT_FALSE(m_state.planner()->active());
        }
    }
}

TEST_F(MobileScreensTest, FormAndSettingsRemainScrollableWithKeyboard)
{
    populate();
    m_view.resize(360, kKeyboardHeight);
    auto* shell = m_view.rootObject();
    shell->setProperty("keyboardVisible", true);
    shell->setProperty("safeTop", 24);
    shell->setProperty("safeBottom", 28);
    m_state.showMore(MoreSection::Settings);
    settle();
    auto* settings = item("mobileSettingsScroll");
    auto* save = item("mobileSaveSettings");
    const auto savePosition = save->mapToItem(settings->property("contentItem").value<QQuickItem*>(), QPointF{}).y();
    settings->setProperty("contentY", std::max(0.0, savePosition + save->height() - settings->height()));
    settle();
    EXPECT_LE(save->mapToScene(QPointF{0, save->height()}).y(), kKeyboardHeight - 28);
    EXPECT_TRUE(saveRender("mobile-settings-keyboard-360-preview.png"));
    m_state.navigate(MobilePage::Plans);
    activate(item("mobileNewTask"));
    settle();
    auto* scroll = item("plannerFormScroll")->property("contentItem").value<QQuickItem*>();
    ASSERT_NE(scroll, nullptr);
    scroll->setProperty("contentY", std::max(0.0, scroll->property("contentHeight").toDouble() - scroll->height()));
    settle();
    const auto* submit = item("plannerSubmitButton");
    EXPECT_GE(submit->mapToScene(QPointF{}).y(), 24);
    EXPECT_LE(submit->mapToScene(QPointF{0, submit->height()}).y(), kKeyboardHeight - 28);
    EXPECT_TRUE(saveRender("mobile-form-keyboard-360-preview.png"));
    m_state.planner()->cancel();
}

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
    for (const auto& entry : m_state.sections())
    {
        const auto section = entry.toMap();
        activate(item(QStringLiteral("mobileNavigation_") + section.value("key").toString()));
        EXPECT_EQ(m_state.page(), section.value("section").value<MobilePage>());
    }
    m_state.showMore(MoreSection::Assistant);
    activate(item("mobileConversationsButton"));
    EXPECT_TRUE(m_state.conversationsVisible());
    ASSERT_TRUE(m_state.selectConversation("2"));
    EXPECT_FALSE(m_state.conversationsVisible());
    EXPECT_EQ(m_state.draft(), QString::fromUtf8("Черновик 🙂"));
    EXPECT_FALSE(item("mobileSendButton")->isEnabled());
    EXPECT_TRUE(m_state.goBack());
    EXPECT_EQ(m_state.moreSection(), MoreSection::Menu);
    EXPECT_TRUE(m_state.goBack());
    EXPECT_EQ(m_state.page(), MobilePage::Today);
    EXPECT_FALSE(m_state.goBack());
    m_state.navigate(static_cast<MobilePage>(-1));
    m_state.navigate(static_cast<MobilePage>(99));
    EXPECT_EQ(m_state.page(), MobilePage::Today);
}
TEST_F(MobileScreensTest, SharedSettingsValidateMaskTokenAndRequireSavedParametersToConnect)
{
    m_state.showMore(MoreSection::Settings);
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
    m_state.navigate(MobilePage::Plans);
    activate(item("mobileNewTask"));
    auto* editor = m_state.planner();
    ASSERT_TRUE(editor->active());
    editor->setField("title", "Новая задача");
    settle();
    EXPECT_TRUE(saveRender("mobile-planner-preview.png"));
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
    struct ScreenCase
    {
        MobilePage page;
        MoreSection nested;
        const char* key;
    };
    for (const auto width : {320, 360, 412})
    {
        m_view.resize(width, kPhoneHeight);
        for (const auto& screen : {ScreenCase{MobilePage::Today, MoreSection::Menu, "today"},
                                   ScreenCase{MobilePage::Plans, MoreSection::Menu, "plans"},
                                   ScreenCase{MobilePage::Sport, MoreSection::Menu, "sport"},
                                   ScreenCase{MobilePage::More, MoreSection::Menu, "more"},
                                   ScreenCase{MobilePage::More, MoreSection::Assistant, "assistant"},
                                   ScreenCase{MobilePage::More, MoreSection::Settings, "settings"}})
        {
            if (screen.page == MobilePage::More)
            {
                m_state.showMore(screen.nested);
            }
            else
            {
                m_state.navigate(screen.page);
            }
            settle();
            auto* area = item("mobilePageArea");
            const auto areaBounds = area->mapRectToScene(QRectF{0, 0, area->width(), area->height()});
            auto* navigation = item("mobileBottomNavigation");
            EXPECT_LE(areaBounds.bottom(), navigation->mapToScene(QPointF{}).y());
            for (const auto& entry : m_state.sections())
            {
                auto* button = item(QStringLiteral("mobileNavigation_") + entry.toMap().value("key").toString());
                ASSERT_NE(button, nullptr);
                const auto bounds = button->mapRectToScene(QRectF{0, 0, button->width(), button->height()});
                EXPECT_GE(bounds.left(), 0);
                EXPECT_LE(bounds.right(), width);
                EXPECT_GE(bounds.height(), kControlHeight);
                EXPECT_GE(bounds.width(), kControlHeight);
            }
            EXPECT_TRUE(
                saveRender(QStringLiteral("mobile-%1-%2-preview.png").arg(QString::fromLatin1(screen.key)).arg(width)));
        }
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
    EXPECT_TRUE(saveRender("mobile-keyboard-preview.png"));
}
} // namespace
