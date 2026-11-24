#include "msa/client/SportController.h"
#include "msa/infra/SportCatalog.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/DesktopState.h"
#include "msa/ui/MobileState.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickView>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace
{
[[nodiscard]] QString makeProgram(msa::ui::SportLibraryModel& library, const QString& exercise = "sport-03")
{
    auto* journal = library.journal();
    journal->createProgram();
    auto* editor = journal->programEditor();
    editor->setField("title", "Домашняя программа — пример теста");
    editor->setField(QString::fromStdString(editor->checkpoint().program.sessions.front().identifier + "/title"),
                     "Занятие A");
    editor->requestExercise(exercise);
    const auto& item = editor->checkpoint().program.sessions.front().items.front();
    editor->setField(QString::fromStdString(item.identifier + "/amplitude"), "Одинаковый ориентир движения");
    editor->setField(QString::fromStdString(item.sets.front().identifier + "/reps"), "8");
    const auto identifier = QString::fromStdString(editor->checkpoint().program.identifier);
    editor->save();
    if (editor->active())
    {
        throw std::runtime_error{editor->status().toStdString()};
    }
    return identifier;
}
void finishOneSet(msa::ui::SportWorkoutEditor& editor, int repetitions)
{
    const auto item = editor.checkpoint().draft.items.front();
    const auto identifier = QString::fromStdString(item.actual.front().identifier);
    editor.setField(identifier + "/reps", QString::number(repetitions));
    editor.recordSet(QString::fromStdString(item.plan.identifier), identifier);
    editor.finish();
}
void settle()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 250)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
}
[[nodiscard]] bool frame(QQuickView& view, const QString& path)
{
    const auto capture = view.contentItem()->grabToImage();
    if (!capture)
    {
        return false;
    }
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
[[nodiscard]] QQuickItem* findItem(QQuickItem* root, const QString& name)
{
    if (root->isVisible() && root->objectName() == name)
    {
        return root;
    }
    for (auto* child : root->childItems())
    {
        if (auto* item = findItem(child, name))
        {
            return item;
        }
    }
    return nullptr;
}
void checkWidth(QQuickItem* item, QQuickItem* window, int width)
{
    if (item->isVisible() && item->property("text").isValid() && item->width() > 0)
    {
        const auto position = item->mapToItem(window, QPointF{});
        EXPECT_GE(position.x(), -1) << item->property("text").toString().toStdString();
        EXPECT_LE(position.x() + item->width(), width + 1) << item->property("text").toString().toStdString();
    }
    for (auto* child : item->childItems())
    {
        checkWidth(child, window, width);
    }
}
} // namespace
TEST(SportWorkoutFlowTest, OfflineWorkoutRestoresRawInputFinishesAndCorrectsCurrentProgress)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    QString workoutIdentifier;
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        auto* journal = library.journal();
        const auto program = makeProgram(library);
        journal->setRestrictions("Индивидуальная заметка до начала");
        journal->saveRestrictions();
        journal->startWorkout(program, 0);
        journal->setRestrictions("Заметка после начала");
        journal->saveRestrictions();
        auto* editor = journal->workoutEditor();
        ASSERT_TRUE(editor->active());
        ASSERT_TRUE(editor->saved());
        workoutIdentifier = QString::fromStdString(editor->checkpoint().draft.identifier);
        const auto setIdentifier = editor->checkpoint().draft.items.front().actual.front().identifier;
        editor->setField(QString::fromStdString(setIdentifier + "/reps"), "1,");
        editor->setField("notes", "Занятие без сервера");
        journal->startWorkout(program, 0);
        EXPECT_EQ(QString::fromStdString(editor->checkpoint().draft.identifier), workoutIdentifier);
        EXPECT_FALSE(journal->errorText().isEmpty());
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    auto* journal = library.journal();
    auto* editor = journal->workoutEditor();
    ASSERT_TRUE(editor->active());
    ASSERT_TRUE(editor->saved());
    const auto item = editor->checkpoint().draft.items.front();
    EXPECT_EQ(msa::ui::sportForm::text(editor->checkpoint().editorText, item.actual.front().identifier + "/reps"),
              "1,");
    const auto absences = editor->checkpoint().draft.timer.absences;
    ASSERT_FALSE(absences.empty());
    EXPECT_TRUE(editor->timeSummary().value("unresolved").toBool());
    for (const auto& interval : absences)
    {
        editor->resolveAbsence(QString::fromStdString(interval.identifier), "pause");
    }
    EXPECT_FALSE(editor->timeSummary().value("unresolved").toBool());
    editor->toggleArea("before", "back");
    editor->toggleArea("before", "other");
    editor->setField("before/other", "Текст другой области");
    editor->setField("before/pain", "2");
    editor->setField("before/newPain", "yes");
    editor->setField("after/pain", "0");
    editor->setField("duration", "900");
    editor->setField("date", "2026-11-24");
    finishOneSet(*editor, 8);
    ASSERT_FALSE(editor->active()) << editor->status().toStdString();
    ASSERT_EQ(journal->savedHistory().size(), 1);
    const auto original = journal->savedHistory().front().current;
    EXPECT_EQ(original.workout.before.areas.size(), 2);
    EXPECT_EQ(original.workout.before.newPain, true);
    EXPECT_EQ(original.workout.after.pain, 0);
    EXPECT_FALSE(original.workout.after.newPain);
    EXPECT_EQ(original.workout.notes, "Занятие без сервера");
    EXPECT_EQ(original.workout.restrictionsSnapshot, "Индивидуальная заметка до начала");
    EXPECT_EQ(original.workout.timer.manualDuration, std::chrono::seconds{900});
    ASSERT_EQ(journal->progress().size(), 1);
    editor->beginCorrection();
    editor->setField("duration", "1200");
    editor->setField(QString::fromStdString(item.actual.front().identifier + "/reps"), "10");
    editor->finish();
    ASSERT_FALSE(editor->correcting()) << editor->status().toStdString();
    ASSERT_EQ(journal->savedHistory().size(), 1);
    const auto current = journal->savedHistory().front().current;
    EXPECT_EQ(current.workout.identifier, original.workout.identifier);
    EXPECT_EQ(current.parentRevision, original.identifier);
    EXPECT_NE(current.identifier, original.identifier);
    EXPECT_EQ(current.workout.timer.begin, original.workout.timer.begin);
    EXPECT_EQ(current.workout.timer.observed, original.workout.timer.observed);
    EXPECT_EQ(current.workout.items.front().actual.front().repetitions, 10);
    EXPECT_EQ(current.workout.timer.manualDuration, std::chrono::seconds{1200});
    EXPECT_TRUE(
        journal->progress().front().toMap().value("points").toList().front().toMap().value("text").toString().contains(
            "10 повт."));
    msa::infra::SqliteSportStore reopened{path};
    EXPECT_FALSE(reopened.activeWorkout());
    EXPECT_EQ(reopened.workoutHistory().size(), 1);
}
TEST(SportWorkoutFlowTest, CheckpointAndFinishFailuresKeepInputAndSkippedWorkoutDoesNotAffectProgress)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    const auto program = makeProgram(library);
    library.journal()->startWorkout(program, 0);
    auto* editor = library.journal()->workoutEditor();
    const auto name = "sport_flow_failure_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        QSqlQuery query{database};
        ASSERT_TRUE(query.exec(
            "CREATE TRIGGER fail_checkpoint BEFORE UPDATE ON checkpoints BEGIN SELECT RAISE(ABORT,'test'); END"));
        editor->setField("notes", "Сохранить несмотря на отказ");
        EXPECT_FALSE(editor->saved());
        EXPECT_EQ(editor->checkpoint().editorText.at("notes"), "Сохранить несмотря на отказ");
        editor->close();
        EXPECT_TRUE(editor->visible());
        ASSERT_TRUE(query.exec("DROP TRIGGER fail_checkpoint"));
        editor->retry();
        EXPECT_TRUE(editor->saved());
        editor->skipExercise(QString::fromStdString(editor->checkpoint().draft.items.front().plan.identifier));
        ASSERT_TRUE(
            query.exec("CREATE TRIGGER fail_finish BEFORE INSERT ON workouts BEGIN SELECT RAISE(ABORT,'test'); END"));
        editor->finish();
        EXPECT_TRUE(editor->active());
        EXPECT_TRUE(library.journal()->savedHistory().empty());
        EXPECT_TRUE(editor->status().contains("не сохранён"));
        ASSERT_TRUE(query.exec("DROP TRIGGER fail_finish"));
        editor->finish();
        ASSERT_FALSE(editor->active()) << editor->status().toStdString();
        ASSERT_EQ(library.journal()->savedHistory().size(), 1);
        EXPECT_TRUE(library.journal()->savedHistory().front().current.entirelySkipped);
        EXPECT_FALSE(
            library.journal()->savedHistory().front().current.workout.items.front().actual.front().repetitions);
        EXPECT_TRUE(library.journal()->progress().empty());
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}
TEST(SportWorkoutFlowTest, LifecyclePauseAndRepeatedManualAbsenceResolutionPreserveRawTime)
{
    QTemporaryDir directory;
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, directory.filePath("sport.sqlite")};
    controller.start();
    library.journal()->startWorkout(makeProgram(library), 0);
    auto* editor = library.journal()->workoutEditor();
    const auto begin = editor->checkpoint().draft.timer.begin;
    editor->pauseOrResume();
    EXPECT_TRUE(editor->checkpoint().draft.timer.paused);
    controller.suspendWorkout();
    controller.suspendWorkout();
    controller.resumeWorkout();
    controller.resumeWorkout();
    ASSERT_EQ(editor->checkpoint().draft.timer.absences.size(), 1);
    const auto interval = editor->checkpoint().draft.timer.absences.front();
    EXPECT_FALSE(interval.decision);
    editor->setField(QString::fromStdString(interval.identifier + "/activeSeconds"), "0");
    editor->resolveAbsence(QString::fromStdString(interval.identifier), "manual");
    editor->resolveAbsence(QString::fromStdString(interval.identifier), "manual");
    const auto& current = editor->checkpoint().draft.timer;
    EXPECT_EQ(current.begin, begin);
    EXPECT_EQ(current.absences.front().begin, interval.begin);
    EXPECT_EQ(current.absences.front().end, interval.end);
    EXPECT_EQ(current.absences.front().active, std::chrono::milliseconds{0});
    EXPECT_TRUE(current.paused);
    EXPECT_FALSE(editor->timeSummary().value("unresolved").toBool());
}
TEST(SportWorkoutFlowTest, RestoreAfterBackwardClockAllowsManualResolutionWithoutNegativeTime)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        library.journal()->startWorkout(makeProgram(library), 0);
    }
    msa::core::SportInstant begin;
    {
        msa::infra::SqliteSportStore store{path};
        auto checkpoint = *store.activeWorkout();
        begin = msa::core::SportInstant{msa::core::SportDuration{QDateTime::currentMSecsSinceEpoch()}} +
                std::chrono::hours{1};
        checkpoint.draft.timer = msa::core::startSportTimer(begin);
        static_cast<void>(store.checkpoint(checkpoint, checkpoint.sequence));
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    auto* editor = library.journal()->workoutEditor();
    ASSERT_TRUE(editor->active());
    EXPECT_TRUE(editor->timeSummary().value("warning").toBool());
    const auto interval = editor->checkpoint().draft.timer.absences.front();
    EXPECT_FALSE(interval.duration);
    editor->resolveAbsence(QString::fromStdString(interval.identifier), "activity");
    EXPECT_TRUE(editor->timeSummary().value("unresolved").toBool());
    editor->setField(QString::fromStdString(interval.identifier + "/activeSeconds"), "0");
    editor->resolveAbsence(QString::fromStdString(interval.identifier), "manual");
    EXPECT_FALSE(editor->timeSummary().value("unresolved").toBool());
    EXPECT_EQ(editor->checkpoint().draft.timer.begin, begin);
    EXPECT_GE(msa::core::summarizeSportDuration(editor->checkpoint().draft.timer).active.count(), 0);
}
TEST(SportWorkoutFlowTest, ConcurrentResultCorrectionIsRejectedAndExplicitCancelKeepsSavedRevision)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::ui::SportLibraryModel first;
    msa::client::SportController firstController{first, path};
    firstController.start();
    first.journal()->startWorkout(makeProgram(first), 0);
    finishOneSet(*first.journal()->workoutEditor(), 8);
    ASSERT_EQ(first.journal()->savedHistory().size(), 1);
    const auto identifier = QString::fromStdString(first.journal()->savedHistory().front().current.workout.identifier);
    msa::ui::SportLibraryModel second;
    msa::client::SportController secondController{second, path};
    secondController.start();
    first.journal()->openWorkout(identifier);
    second.journal()->openWorkout(identifier);
    auto* editor = first.journal()->workoutEditor();
    editor->beginCorrection();
    const auto setKey =
        QString::fromStdString(editor->checkpoint().draft.items.front().actual.front().identifier + "/reps");
    editor->setField(setKey, "9");
    editor->finish();
    auto* stale = second.journal()->workoutEditor();
    stale->beginCorrection();
    stale->setField(setKey, "10");
    stale->finish();
    EXPECT_TRUE(stale->correcting());
    EXPECT_TRUE(stale->status().contains("уже изменён"));
    EXPECT_EQ(stale->checkpoint().editorText.at(setKey.toStdString()), "10");
    stale->requestDiscard();
    EXPECT_TRUE(stale->discardRequested());
    stale->keepEditing();
    EXPECT_FALSE(stale->discardRequested());
    stale->requestDiscard();
    stale->discardCorrection();
    EXPECT_FALSE(stale->correcting());
    stale->close();
    second.journal()->openWorkout(identifier);
    EXPECT_EQ(stale->checkpoint().draft.items.front().actual.front().repetitions, 9);
    msa::infra::SqliteSportStore store{path};
    EXPECT_EQ(store.workoutHistory().front().current.workout.items.front().actual.front().repetitions, 9);
}
TEST(SportWorkoutFlowTest, ReplacementKeepsPerformedFactAndOriginalProgramSnapshot)
{
    QTemporaryDir directory;
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, directory.filePath("sport.sqlite")};
    controller.start();
    const auto program = makeProgram(library);
    library.journal()->startWorkout(program, 0);
    auto* editor = library.journal()->workoutEditor();
    const auto first = editor->checkpoint().draft.items.front();
    editor->setField(QString::fromStdString(first.actual.front().identifier + "/reps"), "8");
    editor->recordSet(QString::fromStdString(first.plan.identifier),
                      QString::fromStdString(first.actual.front().identifier));
    editor->setField(QString::fromStdString(first.plan.identifier + "/replacementSets"), "1");
    editor->setField(QString::fromStdString(first.plan.identifier + "/replacementReps"), "6");
    editor->requestReplacement(QString::fromStdString(first.plan.identifier), "sport-06");
    ASSERT_EQ(editor->checkpoint().draft.items.size(), 2);
    EXPECT_EQ(editor->checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Performed);
    EXPECT_EQ(editor->checkpoint().draft.items.back().replacesItem, first.plan.identifier);
    EXPECT_EQ(editor->checkpoint().draft.originalPlan.items.size(), 1);
    const auto replacement = editor->checkpoint().draft.items.back();
    editor->setField(QString::fromStdString(replacement.actual.front().identifier + "/reps"), "6");
    editor->recordSet(QString::fromStdString(replacement.plan.identifier),
                      QString::fromStdString(replacement.actual.front().identifier));
    editor->finish();
    ASSERT_FALSE(editor->active()) << editor->status().toStdString();
    EXPECT_EQ(library.journal()->savedPrograms().front().sessions.front().items.size(), 1);
    EXPECT_EQ(library.journal()->savedHistory().front().current.workout.items.size(), 2);
    EXPECT_EQ(library.journal()->progress().size(), 2);
    library.journal()->setProgressExercise("sport-06");
    EXPECT_EQ(library.journal()->progress().size(), 1);
    library.journal()->setProgressExercise("");
    EXPECT_EQ(library.journal()->progress().size(), 2);
}
TEST(SportWorkoutFlowTest, ResumedProgramDraftKeepsOriginalExpectedRevision)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    std::string identifier;
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        const auto program = makeProgram(library);
        identifier = program.toStdString();
        library.journal()->editProgram(program);
        library.journal()->programEditor()->setField("title", "Старая правка");
        msa::infra::SqliteSportStore other{path};
        auto updated = other.programs().front();
        const auto expected = updated.revision;
        updated.revision = "other-device-program-revision";
        updated.title = "Чужая новая редакция";
        other.saveProgram(updated, expected);
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    library.journal()->resumeProgram(QString::fromStdString(identifier));
    auto* editor = library.journal()->programEditor();
    editor->save();
    EXPECT_TRUE(editor->active());
    EXPECT_TRUE(editor->status().contains("уже изменилась"));
    msa::infra::SqliteSportStore store{path};
    EXPECT_EQ(store.programs().front().title, "Чужая новая редакция");
}
TEST(SportWorkoutFlowTest, MobileWorkoutAndResultsRenderAndBackPreservesActiveDraft)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::MobileState mobile{client};
    msa::client::SportController controller{*mobile.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    mobile.navigate(msa::ui::MobileState::Page::Sport);
    const auto program = makeProgram(*mobile.sport());
    mobile.sport()->journal()->startWorkout(program, 0);
    auto* editor = mobile.sport()->journal()->workoutEditor();
    controller.suspendWorkout();
    controller.resumeWorkout();
    const auto manualGap = editor->checkpoint().draft.timer.absences.back().identifier;
    QQuickView view;
    QStringList warnings;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &view,
                     [&warnings](const QList<QQmlError>& errors)
                     {
                         for (const auto& error : errors)
                         {
                             warnings.append(error.toString());
                         }
                     });
    view.setInitialProperties({{"client", QVariant::fromValue(&client)}, {"mobile", QVariant::fromValue(&mobile)}});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
    ASSERT_EQ(view.status(), QQuickView::Ready) << view.errors().value(0).toString().toStdString();
    view.resize(360, 844);
    view.show();
    settle();
    const auto fieldName = QString::fromStdString("sportField_" + manualGap + "/activeSeconds");
    QPointer<QQuickItem> manualInput = findItem(view.contentItem(), fieldName);
    ASSERT_NE(manualInput, nullptr);
    manualInput->forceActiveFocus();
    ASSERT_TRUE(manualInput->setProperty("text", "0"));
    ASSERT_TRUE(QMetaObject::invokeMethod(manualInput.data(), "textEdited"));
    QElapsedTimer liveTick;
    liveTick.start();
    while (liveTick.elapsed() < 1100)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }
    ASSERT_NE(manualInput, nullptr);
    EXPECT_EQ(findItem(view.contentItem(), fieldName), manualInput.data());
    EXPECT_TRUE(manualInput->hasActiveFocus());
    editor->resolveAbsence(QString::fromStdString(manualGap), "manual");
    for (const auto width : {320, 360, 412})
    {
        view.resize(width, 844);
        settle();
        checkWidth(view.contentItem(), view.contentItem(), width);
        EXPECT_TRUE(frame(view, QString{"sport-mobile-workout-%1.png"}.arg(width)));
    }
    const auto item = editor->checkpoint().draft.items.front();
    editor->showTechnique(QString::fromStdString(item.plan.identifier));
    EXPECT_TRUE(mobile.sport()->hasSelection());
    EXPECT_TRUE(mobile.goBack());
    EXPECT_FALSE(mobile.sport()->hasSelection());
    EXPECT_TRUE(editor->visible());
    EXPECT_TRUE(mobile.goBack());
    EXPECT_FALSE(editor->visible());
    EXPECT_TRUE(editor->active());
    editor->resume();
    const auto gaps = editor->checkpoint().draft.timer.absences;
    for (const auto& gap : gaps)
    {
        editor->resolveAbsence(QString::fromStdString(gap.identifier), "pause");
    }
    finishOneSet(*editor, 8);
    ASSERT_FALSE(editor->active()) << editor->status().toStdString();
    settle();
    EXPECT_TRUE(frame(view, "sport-mobile-result-412.png"));
    editor->close();
    mobile.sport()->navigate("progress");
    settle();
    EXPECT_TRUE(frame(view, "sport-mobile-progress-412.png"));
    EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
}
TEST(SportWorkoutFlowTest, DesktopWorkoutRendersWithoutHorizontalOverflow)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::DesktopState desktop{client};
    msa::client::SportController controller{*desktop.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    desktop.navigate(msa::ui::DesktopState::Page::Workouts);
    desktop.sport()->journal()->startWorkout(makeProgram(*desktop.sport(), "sport-07"), 0);
    QQuickView view;
    QStringList warnings;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &view,
                     [&warnings](const QList<QQmlError>& errors)
                     {
                         for (const auto& error : errors)
                         {
                             warnings.append(error.toString());
                         }
                     });
    view.setInitialProperties({{"client", QVariant::fromValue(&client)}, {"desktop", QVariant::fromValue(&desktop)}});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Desktop/DesktopShell.qml"});
    ASSERT_EQ(view.status(), QQuickView::Ready);
    view.show();
    for (const auto width : {1000, 1280, 1600})
    {
        view.resize(width, width == 1000 ? 720 : 900);
        settle();
        EXPECT_TRUE(frame(view, QString{"sport-desktop-workout-%1.png"}.arg(width)));
        const auto setIdentifier =
            desktop.sport()->journal()->workoutEditor()->checkpoint().draft.items.front().actual.front().identifier;
        auto* mass = findItem(view.contentItem(), QString::fromStdString("sportField_" + setIdentifier + "/mass"));
        auto* quantity =
            findItem(view.contentItem(), QString::fromStdString("sportField_" + setIdentifier + "/quantity"));
        ASSERT_NE(mass, nullptr);
        ASSERT_NE(quantity, nullptr);
        EXPECT_TRUE(quantity->property("readOnly").toBool());
        EXPECT_NE(mass->property("inputMethodHints").toInt() & Qt::ImhFormattedNumbersOnly, 0);
    }
    EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
}

TEST(SportWorkoutFlowTest, HistoryAndCorrectionDoNotInterruptActiveWorkoutAndSurviveRestart)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    QString historyIdentifier, activeIdentifier;
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        const auto program = makeProgram(library);
        library.journal()->startWorkout(program, 0);
        finishOneSet(*library.journal()->workoutEditor(), 8);
        ASSERT_EQ(library.journal()->savedHistory().size(), 1);
        historyIdentifier =
            QString::fromStdString(library.journal()->savedHistory().front().current.workout.identifier);
        library.journal()->startWorkout(program, 0);
        auto* active = library.journal()->workoutEditor();
        activeIdentifier = QString::fromStdString(active->checkpoint().draft.identifier);
        library.journal()->openWorkout(historyIdentifier);
        auto* result = library.journal()->resultEditor();
        EXPECT_TRUE(active->active());
        EXPECT_FALSE(active->visible());
        ASSERT_TRUE(result->visible());
        result->beginCorrection();
        result->setField("notes", "Незавершённая правка");
        const auto set = result->checkpoint().draft.items.front().actual.front().identifier;
        result->setField(QString::fromStdString(set + "/reps"), "1,");
        ASSERT_TRUE(result->saved()) << result->status().toStdString();
        result->close();
        EXPECT_FALSE(result->visible());
        active->resume();
        active->setField("notes", "Отдельное активное занятие");
        EXPECT_EQ(QString::fromStdString(active->checkpoint().draft.identifier), activeIdentifier);
        EXPECT_EQ(library.journal()->savedHistory().front().current.workout.items.front().actual.front().repetitions,
                  8);
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    ASSERT_TRUE(library.journal()->workoutEditor()->active());
    EXPECT_EQ(QString::fromStdString(library.journal()->workoutEditor()->checkpoint().draft.identifier),
              activeIdentifier);
    library.journal()->openWorkout(historyIdentifier);
    auto* result = library.journal()->resultEditor();
    ASSERT_TRUE(result->correcting());
    EXPECT_EQ(result->checkpoint().editorText.at("notes"), "Незавершённая правка");
    const auto set = result->checkpoint().draft.items.front().actual.front().identifier;
    EXPECT_EQ(result->checkpoint().editorText.at(set + "/reps"), "1,");
    result->setField(QString::fromStdString(set + "/reps"), "10");
    result->finish();
    ASSERT_FALSE(result->correcting()) << result->status().toStdString();
    EXPECT_TRUE(library.journal()->workoutEditor()->active());
    EXPECT_TRUE(library.journal()->corrections().empty());
    EXPECT_EQ(library.journal()->savedHistory().front().current.workout.items.front().actual.front().repetitions, 10);
    msa::infra::SqliteSportStore store{path};
    ASSERT_TRUE(store.activeWorkout());
    EXPECT_EQ(store.activeWorkout()->editorText.at("notes"), "Отдельное активное занятие");
}
TEST(SportWorkoutFlowTest, IncompleteEmptyCorrectionRestoresAndCanAddExerciseWithoutChangingOriginalPlan)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    QString identifier;
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        library.journal()->startWorkout(makeProgram(library), 0);
        auto* editor = library.journal()->workoutEditor();
        finishOneSet(*editor, 8);
        identifier = QString::fromStdString(editor->checkpoint().draft.identifier);
        editor->beginCorrection();
        editor->removeItem(QString::fromStdString(editor->checkpoint().draft.items.front().plan.identifier));
        ASSERT_TRUE(editor->saved());
        EXPECT_TRUE(editor->checkpoint().draft.items.empty());
        editor->finish();
        EXPECT_TRUE(editor->correcting());
        EXPECT_EQ(library.journal()->savedHistory().front().current.workout.items.size(), 1);
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    library.journal()->openWorkout(identifier);
    auto* editor = library.journal()->workoutEditor();
    ASSERT_TRUE(editor->correcting());
    EXPECT_TRUE(editor->checkpoint().draft.items.empty());
    editor->setField("_addition/replacementSets", "1");
    editor->setField("_addition/replacementReps", "6");
    editor->requestAdditionalExercise("sport-06");
    ASSERT_EQ(editor->checkpoint().draft.items.size(), 1);
    finishOneSet(*editor, 6);
    ASSERT_FALSE(editor->correcting()) << editor->status().toStdString();
    EXPECT_EQ(editor->checkpoint().draft.items.front().plan.exercise.identifier, "sport-06");
    EXPECT_EQ(editor->checkpoint().draft.originalPlan.items.front().exercise.identifier, "sport-03");
}
TEST(SportWorkoutFlowTest, BlueprintCopyStartsWithNoDosageAndFirstSetMustBeFilledByUser)
{
    QTemporaryDir directory;
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, directory.filePath("sport.sqlite")};
    controller.start();
    ASSERT_EQ(library.journal()->blueprints().size(), 2);
    library.journal()->copyBlueprint("sport-p1");
    auto* editor = library.journal()->programEditor();
    ASSERT_TRUE(editor->active());
    ASSERT_TRUE(editor->saved());
    const auto item = editor->checkpoint().program.sessions.front().items.front();
    EXPECT_TRUE(item.sets.empty());
    editor->requestSet(QString::fromStdString(item.identifier));
    ASSERT_EQ(editor->checkpoint().program.sessions.front().items.front().sets.size(), 1);
    const auto set = editor->checkpoint().program.sessions.front().items.front().sets.front();
    EXPECT_TRUE(msa::ui::sportForm::text(editor->checkpoint().editorText, set.identifier + "/reps").isEmpty());
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(editor->candidate()));
    EXPECT_TRUE(library.journal()->savedPrograms().empty());
}
TEST(SportWorkoutFlowTest, ProgramTechniqueActuallyOpensAndReturnsToEditorAtPhoneWidth)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::MobileState mobile{client};
    msa::client::SportController controller{*mobile.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    mobile.navigate(msa::ui::MobileState::Page::Sport);
    mobile.sport()->journal()->createProgram();
    auto* editor = mobile.sport()->journal()->programEditor();
    editor->requestExercise("sport-03");
    const auto item = editor->checkpoint().program.sessions.front().items.front();
    editor->showTechnique(QString::fromStdString(item.identifier));
    QQuickView view;
    view.setInitialProperties({{"client", QVariant::fromValue(&client)}, {"mobile", QVariant::fromValue(&mobile)}});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
    ASSERT_EQ(view.status(), QQuickView::Ready);
    view.resize(360, 844);
    view.show();
    settle();
    EXPECT_NE(findItem(view.contentItem(), "sportDetails"), nullptr);
    EXPECT_TRUE(mobile.goBack());
    EXPECT_FALSE(mobile.sport()->hasSelection());
    EXPECT_TRUE(editor->active());
    EXPECT_FALSE(mobile.sport()->showExercises());
}
TEST(SportWorkoutFlowTest, ExistingDatabaseMigrationRequiresExplicitUserAction)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    {
        msa::infra::SqliteSportStore store{path};
    }
    const auto name = QUuid::createUuid().toString();
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        QSqlQuery query{database};
        ASSERT_TRUE(query.exec("PRAGMA user_version=4"));
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const auto before = file.readAll();
    file.close();
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    EXPECT_TRUE(library.migrationRequired());
    EXPECT_FALSE(library.storageReady());
    EXPECT_EQ(library.exercises().size(), 22);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), before);
    file.close();
    EXPECT_FALSE(QFileInfo::exists(directory.filePath("backups")));
    library.approveMigration();
    EXPECT_FALSE(library.migrationRequired());
    EXPECT_TRUE(library.storageReady()) << library.errorText().toStdString();
    EXPECT_TRUE(QFileInfo::exists(directory.filePath("backups")));
}

TEST(SportWorkoutFlowTest, DamagedCorrectionKeepsLastHistoryAndOpeningItReportsFailureWithoutCrash)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    library.journal()->startWorkout(makeProgram(library), 0);
    auto* editor = library.journal()->workoutEditor();
    finishOneSet(*editor, 8);
    const auto identifier = QString::fromStdString(editor->checkpoint().draft.identifier);
    editor->beginCorrection();
    editor->setField("notes", "Сохранённая правка");
    editor->close();
    const auto name = QUuid::createUuid().toString();
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        QSqlQuery query{database};
        ASSERT_TRUE(query.exec("UPDATE correction_drafts SET payload='{}'"));
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    library.retry();
    EXPECT_FALSE(library.storageReady());
    ASSERT_EQ(library.journal()->savedHistory().size(), 1);
    library.journal()->openWorkout(identifier);
    EXPECT_FALSE(library.journal()->errorText().isEmpty());
    EXPECT_EQ(library.journal()->savedHistory().front().current.workout.items.front().actual.front().repetitions, 8);
}

TEST(SportWorkoutFlowTest, CorrectionDiscardPromptStaysVisibleAtTopOfPhoneForm)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::MobileState mobile{client};
    msa::client::SportController controller{*mobile.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    mobile.navigate(msa::ui::MobileState::Page::Sport);
    mobile.sport()->journal()->startWorkout(makeProgram(*mobile.sport()), 0);
    auto* editor = mobile.sport()->journal()->workoutEditor();
    finishOneSet(*editor, 8);
    editor->beginCorrection();
    editor->setField("notes", "Черновик исправления");
    editor->requestDiscard();
    QQuickView view;
    view.setInitialProperties({{"client", QVariant::fromValue(&client)}, {"mobile", QVariant::fromValue(&mobile)}});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl{"qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
    ASSERT_EQ(view.status(), QQuickView::Ready);
    view.resize(360, 844);
    view.show();
    settle();
    auto* button = findItem(view.contentItem(), "sportDiscardCorrectionConfirm");
    ASSERT_NE(button, nullptr);
    const auto position = button->mapToItem(view.contentItem(), QPointF{});
    EXPECT_GE(position.y(), 0);
    EXPECT_LE(position.y() + button->height(), view.height());
    EXPECT_TRUE(frame(view, "sport-mobile-correction-confirm-360.png"));
    editor->discardCorrection();
    EXPECT_FALSE(editor->correcting());
    EXPECT_TRUE(mobile.sport()->journal()->corrections().empty());
}

TEST(SportWorkoutFlowTest, MvpProgramsHistoryProgressAndParallelCorrectionRenderOnBothClients)
{
    QTemporaryDir directory;
    for (const bool desktopView : {true, false})
    {
        msa::ui::ClientModels client;
        msa::ui::DesktopState desktop{client};
        msa::ui::MobileState mobile{client};
        auto* library = desktopView ? desktop.sport() : mobile.sport();
        msa::client::SportController controller{*library,
                                                directory.filePath(desktopView ? "desktop.sqlite" : "mobile.sqlite")};
        controller.start();
        if (desktopView)
        {
            desktop.navigate(msa::ui::DesktopState::Page::Workouts);
        }
        else
        {
            mobile.navigate(msa::ui::MobileState::Page::Sport);
        }
        library->openPrograms();
        QQuickView view;
        QStringList warnings;
        QObject::connect(view.engine(), &QQmlEngine::warnings, &view,
                         [&warnings](const QList<QQmlError>& errors)
                         {
                             for (const auto& error : errors)
                             {
                                 warnings.append(error.toString());
                             }
                         });
        QVariantMap properties{{"client", QVariant::fromValue(&client)}};
        if (desktopView)
        {
            properties.insert("desktop", QVariant::fromValue(&desktop));
        }
        else
        {
            properties.insert("mobile", QVariant::fromValue(&mobile));
        }
        view.setInitialProperties(properties);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl{desktopView ? "qrc:/qt/qml/Msa/Ui/Desktop/DesktopShell.qml"
                                        : "qrc:/qt/qml/Msa/Ui/Mobile/MobileShell.qml"});
        ASSERT_EQ(view.status(), QQuickView::Ready) << view.errors().value(0).toString().toStdString();
        view.resize(desktopView ? 1280 : 360, desktopView ? 900 : 844);
        view.show();
        settle();
        const auto prefix = desktopView ? QString{"sport-mvp-desktop-"} : QString{"sport-mvp-mobile-"};
        for (const auto& identifier : {QString{"sport-p1"}, QString{"sport-p3"}})
        {
            auto* card = findItem(view.contentItem(), "sportBlueprint_" + identifier);
            ASSERT_NE(card, nullptr);
            auto* details = findItem(card, "sportBlueprintDetails");
            ASSERT_NE(details, nullptr);
            ASSERT_TRUE(QMetaObject::invokeMethod(details, "clicked"));
        }
        settle();
        EXPECT_TRUE(frame(view, prefix + "programs-top.png"));
        auto* list = findItem(view.contentItem(), "sportProgramsList");
        ASSERT_NE(list, nullptr);
        auto* flick = list->property("contentItem").value<QQuickItem*>();
        ASSERT_NE(flick, nullptr);
        const auto contentHeight = flick->property("contentHeight").toDouble();
        EXPECT_GT(contentHeight, flick->height());
        ASSERT_TRUE(flick->setProperty("contentY", contentHeight - flick->height()));
        settle();
        EXPECT_TRUE(frame(view, prefix + "programs-bottom.png"));
        const auto program = makeProgram(*library);
        library->journal()->startWorkout(program, 0);
        auto* active = library->journal()->workoutEditor();
        finishOneSet(*active, 8);
        ASSERT_FALSE(active->active()) << active->status().toStdString();
        const auto historyId = QString::fromStdString(active->checkpoint().draft.identifier);
        settle();
        EXPECT_TRUE(frame(view, prefix + "result.png"));
        active->close();
        library->navigate("history");
        settle();
        EXPECT_TRUE(frame(view, prefix + "history.png"));
        library->navigate("progress");
        settle();
        EXPECT_TRUE(frame(view, prefix + "progress.png"));
        library->journal()->startWorkout(program, 0);
        library->journal()->openWorkout(historyId);
        auto* result = library->journal()->resultEditor();
        ASSERT_TRUE(result->visible());
        EXPECT_TRUE(active->active());
        EXPECT_FALSE(active->visible());
        result->beginCorrection();
        result->setField("notes", "Черновик исправления на устройстве");
        settle();
        EXPECT_TRUE(frame(view, prefix + "correction-with-active.png"));
        result->showTechnique(QString::fromStdString(result->checkpoint().draft.items.front().plan.identifier));
        settle();
        EXPECT_NE(findItem(view.contentItem(), "sportDetails"), nullptr);
        EXPECT_TRUE(frame(view, prefix + "historical-technique.png"));
        library->clearSelection();
        result->close();
        active->resume();
        settle();
        EXPECT_TRUE(active->active());
        EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
    }
}
