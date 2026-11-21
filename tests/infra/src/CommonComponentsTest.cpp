#include "msa/ui/ClientModels.h"

#include <gtest/gtest.h>

#include <QColor>
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

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{
constexpr int kWaitMilliseconds = 3000;
constexpr int kPreviewWidth = 960;
constexpr int kNarrowWidth = 360;
constexpr int kWideMobileWidth = 412;
constexpr int kMinimumTouchSize = 48;
constexpr int kAnimationSettleMilliseconds = 180;
constexpr double kMinimumTextContrast = 4.5;

void collectVisualItems(QQuickItem* parent, QList<QQuickItem*>& result)
{
    for (auto* child : parent->childItems())
    {
        result.append(child);
        collectVisualItems(child, result);
    }
}

[[nodiscard]] double luminance(const QColor& color)
{
    const auto linear = [](double channel)
    {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

[[nodiscard]] double contrast(const QColor& first, const QColor& second)
{
    const auto firstLuminance = luminance(first);
    const auto secondLuminance = luminance(second);

    return (std::max(firstLuminance, secondLuminance) + 0.05) /
           (std::min(firstLuminance, secondLuminance) + 0.05);
}

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
        const auto objectName = QString::fromLatin1(name);
        auto* parent = objectName == QStringLiteral("reconnectButton")
                           ? m_view.rootObject()->findChild<QQuickItem*>(QStringLiteral("connectionBanner"))
                           : m_view.rootObject();

        return parent->findChild<QQuickItem*>(objectName);
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

    [[nodiscard]] QImage render()
    {
        QElapsedTimer timer;
        timer.start();
        static_cast<void>(waitUntil([&timer] { return timer.elapsed() >= kAnimationSettleMilliseconds; }));

        return m_view.grabWindow();
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
    for (const auto width : {kNarrowWidth, kWideMobileWidth})
    {
        m_view.resize(width, m_view.height());
        settle();
        for (const auto* name : {"taskRow", "habitProgress", "messageBubble", "connectionBanner", "errorBanner",
                                 "readyBanner", "connectingBanner", "emptyState", "inputField", "errorField",
                                 "inputArea", "errorArea", "disabledField", "disabledArea", "primaryButton",
                                 "secondaryButton", "dangerButton", "disabledButton", "reconnectButton"})
        {
            auto* target = item(name);
            ASSERT_NE(target, nullptr);
            const auto left = target->mapToScene(QPointF{}).x();
            EXPECT_GE(left, 0) << name << " at " << width;
            EXPECT_LE(left + target->width(), width) << name << " at " << width;
        }
    }
}

TEST_F(CommonComponentsTest, ControlsKeepTouchTargetsAndReadableDisabledLabels)
{
    for (const auto* name : {"primaryButton", "secondaryButton", "dangerButton", "disabledButton", "inputField",
                             "disabledField", "taskCheckBox", "reconnectButton"})
    {
        const auto* target = item(name);
        ASSERT_NE(target, nullptr);
        EXPECT_GE(target->width(), kMinimumTouchSize) << name;
        EXPECT_GE(target->height(), kMinimumTouchSize) << name;
    }
    auto* primary = item("primaryButton");
    ASSERT_NE(primary, nullptr);
    primary->setProperty("tone", "secondary");
    primary->setProperty("enabled", false);
    for (const auto* name : {"disabledButton", "disabledField", "disabledArea", "primaryButton"})
    {
        const auto* target = item(name);
        ASSERT_NE(target, nullptr);
        EXPECT_FALSE(target->isEnabled());
        auto* background = target->property("background").value<QQuickItem*>();
        ASSERT_NE(background, nullptr);
        const auto* label = target->property("contentItem").value<QQuickItem*>();
        const auto foreground = target->property("color").isValid() ? target->property("color").value<QColor>()
                                                                 : label->property("color").value<QColor>();
        EXPECT_GE(contrast(foreground, background->property("color").value<QColor>()), kMinimumTextContrast) << name;
    }
}

TEST_F(CommonComponentsTest, ErrorCaptionsWrapBelowEditableTextAndPreserveDraft)
{
    for (const auto width : {kNarrowWidth, kWideMobileWidth})
    {
        m_view.resize(width, m_view.height());
        settle();
        for (const auto* name : {"errorField", "errorArea"})
        {
            auto* field = item(name);
            ASSERT_NE(field, nullptr);
            const auto draft = field->property("text").toString();
            QList<QQuickItem*> children;
            collectVisualItems(field, children);
            const auto caption = std::ranges::find_if(children, [](const QQuickItem* child)
            {
                return child->isVisible() && child->property("text").toString().startsWith(QString::fromUtf8("Ошибка: "));
            });
            ASSERT_NE(caption, children.end());
            const auto captionTop = (*caption)->mapToItem(field, QPointF{}).y();
            EXPECT_GE(captionTop, field->height() - field->property("bottomPadding").toDouble());
            EXPECT_LE(captionTop + (*caption)->height(), field->height());
            EXPECT_GE((*caption)->width(), (*caption)->property("contentWidth").toDouble());
            EXPECT_EQ(field->property("text").toString(), draft);
            field->setProperty("errorText", "");
            EXPECT_FALSE((*caption)->property("text").toString().trimmed().endsWith(':'));
        }
    }
}

TEST_F(CommonComponentsTest, KeyboardFocusIsVisibleOnPrimaryButtonAndErrorField)
{
    for (const auto* name : {"primaryButton", "errorField"})
    {
        auto* target = item(name);
        ASSERT_NE(target, nullptr);
        target->forceActiveFocus(Qt::TabFocusReason);
        const auto preview = render();
        ASSERT_FALSE(preview.isNull());
        EXPECT_TRUE(target->hasActiveFocus());
        const auto point = target->mapToScene(QPointF{-3, target->height() / 2});
        const auto ringColor = preview.pixelColor(point.toPoint());
        const auto pageColor = m_view.rootObject()->property("color").value<QColor>();
        EXPECT_GE(contrast(ringColor, pageColor), kMinimumTextContrast) << name;
        if (QString::fromLatin1(name) == QStringLiteral("primaryButton"))
        {
            const auto innerPoint = target->mapToScene(QPointF{1, target->height() / 2});
            // В списке с clip внешнее кольцо может обрезаться: внутренняя рамка сохраняет фокус.
            EXPECT_EQ(preview.pixelColor(innerPoint.toPoint()), ringColor) << name;
        }
        EXPECT_TRUE(preview.save(QString::fromLatin1("common-focus-%1-preview.png").arg(QString::fromLatin1(name))));
    }
}

TEST_F(CommonComponentsTest, RendersComponentGallery)
{
    m_models.setConnectionState(msa::core::ClientConnectionState::Exhausted, 5);
    m_view.rootObject()->setProperty("actionsEnabled", false);
    settle();
    for (const auto width : {kPreviewWidth, kNarrowWidth, kWideMobileWidth})
    {
        m_view.resize(width, m_view.height());
        settle();
        auto* field = item("errorField");
        ASSERT_NE(field, nullptr);
        field->forceActiveFocus(Qt::TabFocusReason);
        const auto preview = render();
        ASSERT_FALSE(preview.isNull());
        EXPECT_EQ(preview.width(), width);
        const auto filename = width == kPreviewWidth ? QStringLiteral("common-components-preview.png")
                                                    : QStringLiteral("common-components-%1-preview.png").arg(width);
        EXPECT_TRUE(preview.save(filename));
    }
}
} // namespace
