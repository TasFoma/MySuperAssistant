#include "msa/ui/ClientModels.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>
#include <cmath>
#include <functional>

namespace
{
constexpr int kWaitMilliseconds = 3000;
constexpr int kPreviewWidth = 960;
constexpr int kNarrowWidth = 360;

bool waitUntil(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < kWaitMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }

    return condition();
}

class CommonComponentsTest : public ::testing::Test
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
        m_view.engine()->rootContext()->setContextProperty("clientState", &m_models);
        m_view.setResizeMode(QQuickView::SizeRootObjectToView);
        m_view.setSource(QUrl{"qrc:/tests/qml/CommonComponentsGallery.qml"});
        ASSERT_EQ(m_view.status(), QQuickView::Ready) << m_view.errors().value(0).toString().toStdString();
        m_view.resize(kPreviewWidth, 1200);
        m_view.show();
        ASSERT_TRUE(waitUntil([this] { return m_view.isExposed(); }));
        settle();
    }

    void TearDown() override
    {
        EXPECT_TRUE(m_warnings.isEmpty()) << m_warnings.join('\n').toStdString();
    }

    void settle()
    {
        m_view.rootObject()->polish();
        QGuiApplication::processEvents();
        m_view.resize(m_view.width(), static_cast<int>(std::ceil(m_view.rootObject()->implicitHeight())));
        QGuiApplication::processEvents();
    }

    [[nodiscard]] QQuickItem* item(const char* name)
    {
        return m_view.rootObject()->findChild<QQuickItem*>(QString::fromLatin1(name));
    }

    void activate(QQuickItem* target)
    {
        target->forceActiveFocus(Qt::TabFocusReason);
        QKeyEvent press{QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier};
        QKeyEvent release{QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier};
        QGuiApplication::sendEvent(&m_view, &press);
        QGuiApplication::sendEvent(&m_view, &release);
        QGuiApplication::processEvents();
    }

    msa::ui::ClientModels m_models{};
    QQuickView m_view{};
    QStringList m_warnings{};
};

TEST_F(CommonComponentsTest, KeyboardActivatesButtonsButDisabledButtonDoesNot)
{
    auto* button = item("primaryButton");
    ASSERT_NE(button, nullptr);
    activate(button);
    EXPECT_EQ(m_view.rootObject()->property("primaryClicks").toInt(), 1);
    button->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent press{QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier};
    QKeyEvent release{QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier};
    QGuiApplication::sendEvent(&m_view, &press);
    QGuiApplication::sendEvent(&m_view, &release);
    EXPECT_EQ(m_view.rootObject()->property("primaryClicks").toInt(), 2);
    activate(item("disabledButton"));
    EXPECT_EQ(m_view.rootObject()->property("disabledClicks").toInt(), 0);
}

TEST_F(CommonComponentsTest, InputAcceptsUnicode)
{
    auto* field = item("inputField");
    ASSERT_NE(field, nullptr);
    field->setProperty("text", "");
    field->forceActiveFocus(Qt::TabFocusReason);
    const auto text = QString::fromUtf8("Задача 🙂");
    QKeyEvent input{QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, text};
    QGuiApplication::sendEvent(&m_view, &input);
    EXPECT_EQ(field->property("text").toString(), text);
}

TEST_F(CommonComponentsTest, TaskRequestsChangeAndWaitsForConfirmedData)
{
    auto* check = item("taskCheckBox");
    ASSERT_NE(check, nullptr);
    activate(check);
    EXPECT_EQ(m_view.rootObject()->property("doneRequests").toInt(), 1);
    EXPECT_EQ(m_view.rootObject()->property("requestedIdentifier").toString(), "17");
    EXPECT_TRUE(m_view.rootObject()->property("requestedDone").toBool());
    EXPECT_FALSE(check->property("checked").toBool());
    m_view.rootObject()->setProperty("taskDone", true);
    EXPECT_TRUE(check->property("checked").toBool());
    m_view.rootObject()->setProperty("actionsEnabled", false);
    activate(check);
    EXPECT_EQ(m_view.rootObject()->property("doneRequests").toInt(), 1);
}

TEST_F(CommonComponentsTest, MessageRemainsSelectablePlainTextOffline)
{
    auto* body = item("messageBody");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->property("text").toString(), QString::fromUtf8("<b>Обычный текст</b>\nЧто получилось сегодня? 🙂"));
    EXPECT_EQ(body->property("textFormat").toInt(), 0);
    EXPECT_TRUE(body->property("readOnly").toBool());
    EXPECT_TRUE(body->property("selectByMouse").toBool());
    EXPECT_TRUE(body->isEnabled());
}

TEST_F(CommonComponentsTest, HabitClampsDisplayWithoutChangingSourceAmounts)
{
    auto* habit = item("habitProgress");
    auto* bar = item("habitBar");
    ASSERT_NE(habit, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_DOUBLE_EQ(bar->property("value").toDouble(), 1.5);
    EXPECT_DOUBLE_EQ(habit->property("progress").toDouble(), 3.5);
    EXPECT_EQ(habit->property("amountText").toString(), QString::fromUtf8("3,5 / 1,5 главы"));
}

TEST_F(CommonComponentsTest, ManualReconnectIsAvailableOnlyAfterStoppedCycle)
{
    int requests{0};
    QObject::connect(&m_models, &msa::ui::ClientModels::reconnectRequested, &m_view, [&requests] { ++requests; });
    auto* button = item("reconnectButton");
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(button->isVisible());
    m_models.setConnectionState(msa::core::ClientConnectionState::Connecting, 1);
    EXPECT_FALSE(button->isVisible());
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    settle();
    EXPECT_TRUE(button->isVisible());
    activate(button);
    EXPECT_EQ(requests, 1);
}

TEST_F(CommonComponentsTest, UnknownResultHasNoDefaultRetryAction)
{
    auto* button = item("errorActionButton");
    ASSERT_NE(button, nullptr);
    EXPECT_FALSE(button->isVisible());
    EXPECT_FALSE(button->isEnabled());
}

TEST_F(CommonComponentsTest, NarrowLayoutKeepsCardsInsideViewport)
{
    m_view.resize(kNarrowWidth, m_view.height());
    settle();
    for (const auto* name : {"taskRow", "habitProgress", "messageBubble", "connectionBanner", "errorBanner"})
    {
        auto* card = item(name);
        ASSERT_NE(card, nullptr);
        const auto left = card->mapToScene(QPointF{}).x();
        EXPECT_GE(left, 0) << name;
        EXPECT_LE(left + card->width(), kNarrowWidth) << name;
    }
}

TEST_F(CommonComponentsTest, RendersComponentGallery)
{
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    m_view.rootObject()->setProperty("actionsEnabled", false);
    settle();
    QImage preview;
    ASSERT_TRUE(waitUntil(
        [this, &preview]
        {
            preview = m_view.grabWindow();
            return !preview.isNull();
        }));
    EXPECT_EQ(preview.width(), kPreviewWidth);
    EXPECT_TRUE(preview.save("common-components-preview.png"));
}
} // namespace
