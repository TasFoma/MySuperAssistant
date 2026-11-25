#include "msa/client/SportController.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/DesktopState.h"
#include "msa/ui/MobileState.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickView>
#include <QTemporaryDir>
#include <QThread>

#include <memory>

namespace
{
void settleSportUi()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 200)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
}
[[nodiscard]] QQuickItem* visibleSportItem(QQuickItem* root, const QString& name)
{
    if (root->isVisible() && root->objectName() == name)
    {
        return root;
    }
    for (auto* child : root->childItems())
    {
        if (auto* item = visibleSportItem(child, name))
        {
            return item;
        }
    }

    return nullptr;
}
[[nodiscard]] bool captureSportUi(QQuickView& view, const QString& path)
{
    const auto capture = view.contentItem()->grabToImage();
    if (!capture) return false;
    bool ready{};
    QObject::connect(capture.data(), &QQuickItemGrabResult::ready, &view, [&ready] { ready = true; });
    QElapsedTimer timer;
    timer.start();
    while (!ready && timer.elapsed() < 3000)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }

    return ready && capture->saveToFile(path);
}
[[nodiscard]] QString prepareDailyProgram(msa::ui::SportLibraryModel& sport)
{
    sport.journal()->createProgram();
    auto* editor = sport.journal()->programEditor();
    editor->setField("title", "Домашняя программа");
    const auto session = editor->checkpoint().program.sessions.front().identifier;
    editor->setField(QString::fromStdString(session + "/title"), "Занятие A");
    editor->requestExercise("sport-07");
    const auto item = editor->checkpoint().program.sessions.front().items.front();
    editor->setField(QString::fromStdString(item.identifier + "/amplitude"), "До одного ориентира");
    editor->setField(QString::fromStdString(item.sets.front().identifier + "/reps"), "8");
    editor->setField(QString::fromStdString(item.sets.front().identifier + "/mass"), "5,5");
    const auto identifier = QString::fromStdString(editor->checkpoint().program.identifier);
    editor->save();

    return identifier;
}
} // namespace

TEST(SportDailyUxTest, OfflineOverviewTabsAndWorkoutRemainUsableAcrossSizesAndFontScales)
{
    QTemporaryDir directory;
    for (const bool desktopView : {false, true})
    {
        msa::ui::ClientModels client;
        msa::ui::DesktopState desktop{client};
        msa::ui::MobileState mobile{client};
        auto* sport = desktopView ? desktop.sport() : mobile.sport();
        msa::client::SportController controller{*sport, directory.filePath(desktopView ? "desktop.sqlite" : "mobile.sqlite")};
        controller.start();
        if (desktopView) desktop.navigate(msa::ui::DesktopState::Page::Workouts);
        else mobile.navigate(msa::ui::MobileState::Page::Sport);
        QQuickView view;
        QStringList warnings;
        QObject::connect(view.engine(), &QQmlEngine::warnings, &view,
                         [&warnings](const QList<QQmlError>& errors)
                         {
                             for (const auto& error : errors) warnings.append(error.toString());
                         });
        view.setInitialProperties(desktopView
                                      ? QVariantMap{{"client", QVariant::fromValue(&client)}, {"desktop", QVariant::fromValue(&desktop)}}
                                      : QVariantMap{{"client", QVariant::fromValue(&client)}, {"mobile", QVariant::fromValue(&mobile)}});
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl{desktopView ? "qrc:/qt/qml/Msa/Ui/Desktop/DesktopShell.qml" : "qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
        ASSERT_EQ(view.status(), QQuickView::Ready);
        view.show();
        const auto prefix = desktopView ? QString{"sport-ux-desktop-"} : QString{"sport-ux-mobile-"};
        const auto sizes = desktopView ? QList<int>{1000, 1280, 1600} : QList<int>{320, 360, 412};
        for (const auto width : sizes)
        {
            view.resize(width, desktopView ? 900 : 844);
            settleSportUi();
            EXPECT_EQ(visibleSportItem(view.contentItem(), desktopView ? "desktopConnectionBanner" : "mobileConnectionBanner"), nullptr);
            for (const auto& section : {"overview", "programs", "history", "progress"})
            {
                auto* tab = visibleSportItem(view.contentItem(), "sportSection_" + QString::fromLatin1(section));
                ASSERT_NE(tab, nullptr);
                const auto position = tab->mapToItem(view.contentItem(), QPointF{});
                EXPECT_GE(position.x(), 0);
                EXPECT_LE(position.x() + tab->width(), width);
                EXPECT_GE(tab->height(), 48);
            }
            EXPECT_TRUE(captureSportUi(view, prefix + "overview-" + QString::number(width) + ".png"));
        }
        const auto program = prepareDailyProgram(*sport);
        ASSERT_FALSE(sport->journal()->programEditor()->active());
        sport->journal()->startWorkout(program, 0);
        auto* editor = sport->journal()->workoutEditor();
        ASSERT_TRUE(editor->active());
        const auto item = editor->checkpoint().draft.items.front();
        const auto itemId = QString::fromStdString(item.plan.identifier);
        const auto setId = QString::fromStdString(item.actual.front().identifier);
        editor->reportStorageError("Тестовый отказ локальной записи. Ввод остаётся в форме.");
        settleSportUi();
        auto* storageIssue = visibleSportItem(view.contentItem(), "sportStorageIssue");
        ASSERT_NE(storageIssue, nullptr);
        EXPECT_TRUE(captureSportUi(view, prefix + "storage-error.png"));
        ASSERT_TRUE(QMetaObject::invokeMethod(storageIssue, "actionRequested"));
        settleSportUi();
        EXPECT_EQ(visibleSportItem(view.contentItem(), "sportStorageIssue"), nullptr);
        EXPECT_TRUE(editor->active());
        QQmlComponent scaleComponent{view.engine()};
        scaleComponent.setData("import QtQuick; import Msa.Ui.Common 1.0; QtObject { property real scale: 1; onScaleChanged: Theme.textScale = scale }", QUrl{});
        const std::unique_ptr<QObject> scale{scaleComponent.create()};
        ASSERT_NE(scale, nullptr);
        for (const auto width : sizes)
        {
            for (const auto fontScale : {1.0, 1.3, 1.5})
            {
                scale->setProperty("scale", fontScale);
                view.resize(width, desktopView ? 900 : (fontScale == 1.0 ? 844 : 640));
                settleSportUi();
                auto* repetitions = visibleSportItem(view.contentItem(), "sportField_" + setId + "/reps");
                ASSERT_NE(repetitions, nullptr);
                EXPECT_GE(repetitions->height(), 48);
                if (fontScale == 1.0)
                {
                    const auto position = repetitions->mapToItem(view.contentItem(), QPointF{});
                    EXPECT_LT(position.y() + repetitions->height(), view.height());
                }
                else
                {
                    repetitions->forceActiveFocus();
                    settleSportUi();
                    const auto position = repetitions->mapToItem(view.contentItem(), QPointF{});
                    const auto* viewport = visibleSportItem(view.contentItem(), "sportWorkoutScroll");
                    ASSERT_NE(viewport, nullptr);
                    const auto viewportTop = viewport->mapToItem(view.contentItem(), QPointF{}).y();
                    EXPECT_GE(position.y(), viewportTop);
                    EXPECT_LE(position.y() + repetitions->height(), viewportTop + viewport->height());
                }
                EXPECT_TRUE(captureSportUi(view, prefix + "workout-" + QString::number(width) + "-" + QString::number(qRound(fontScale * 100)) + ".png"));
            }
        }
        scale->setProperty("scale", 1.0);
        editor->close();
        sport->navigate("overview");
        settleSportUi();
        EXPECT_TRUE(captureSportUi(view, prefix + "resume.png"));
        editor->resume();
        editor->usePlan(itemId, setId);
        editor->recordSet(itemId, setId);
        ASSERT_TRUE(editor->saved());
        auto* finish = visibleSportItem(view.contentItem(), "sportFinishWorkout");
        ASSERT_NE(finish, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(finish, "clicked"));
        settleSportUi();
        EXPECT_TRUE(editor->active());
        EXPECT_TRUE(captureSportUi(view, prefix + "preview.png"));
        finish = visibleSportItem(view.contentItem(), "sportFinishWorkout");
        ASSERT_NE(finish, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(finish, "clicked"));
        ASSERT_FALSE(editor->active()) << editor->status().toStdString();
        if (!desktopView)
        {
            sport->journal()->startWorkout(program, 0);
            const auto activeSet = QString::fromStdString(editor->checkpoint().draft.items.front().actual.front().identifier);
            editor->setField(activeSet + "/reps", "8");
            editor->setField(activeSet + "/mass", "1,");
            controller.suspendWorkout();
            ASSERT_TRUE(editor->saved());
            // Изолированный воспроизводимый пример для Android, только из временной тестовой БД.
            const auto target = QDir::current().filePath("sport-ux-qa-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmsszzz") + ".sqlite");
            EXPECT_TRUE(QFile::copy(directory.filePath("mobile.sqlite"), target));
        }
        EXPECT_TRUE(warnings.isEmpty()) << warnings.join('\n').toStdString();
    }
}
