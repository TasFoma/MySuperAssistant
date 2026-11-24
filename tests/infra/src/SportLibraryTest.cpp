#include "msa/client/SportController.h"
#include "msa/infra/SportCatalog.h"
#include "msa/ui/ClientModels.h"
#include "msa/ui/DesktopState.h"
#include "msa/ui/MobileState.h"

#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
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
#include <functional>

namespace
{
constexpr int kWaitMilliseconds = 3000;
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
QQuickItem* findItem(QQuickItem* root, const QString& name)
{
    if (root->objectName() == name && root->isVisible())
    {
        return root;
    }
    for (auto* child : root->childItems())
    {
        if (auto* found = findItem(child, name))
        {
            return found;
        }
    }

    return nullptr;
}
bool saveFrame(QQuickView& view, const QString& path)
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
    while (!ready && timer.elapsed() < kWaitMilliseconds)
    {
        QGuiApplication::processEvents();
        QThread::msleep(1);
    }

    return ready && capture->image().save(path);
}
void rejectPreferenceWrites(const QString& path)
{
    const auto name = QUuid::createUuid().toString();
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        {
            QSqlQuery query{database};
            ASSERT_TRUE(query.exec(
                "CREATE TRIGGER reject_mark BEFORE INSERT ON preferences BEGIN SELECT RAISE(ABORT,'test'); END"));
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}
} // namespace
TEST(SportLibraryTest, PublishesOnlyCompleteSourceCheckedCards)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    ASSERT_EQ(catalog.size(), 22U);
    int prepared{};
    for (const auto& exercise : catalog)
    {
        EXPECT_FALSE(msa::core::validateSportExercise(exercise));
        if (exercise.published)
        {
            ++prepared;
            EXPECT_FALSE(exercise.technique.steps.empty());
            EXPECT_FALSE(exercise.sources.empty());
            EXPECT_FALSE(exercise.technique.brief.empty());
        }
    }
    EXPECT_EQ(prepared, 6);
}
TEST(SportLibraryTest, SearchFiltersAndIndependentMarksSurviveReopen)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    {
        msa::ui::SportLibraryModel model;
        msa::client::SportController controller{model, path};
        controller.start();
        ASSERT_TRUE(model.storageReady()) << model.errorText().toStdString();
        EXPECT_EQ(model.exercises().size(), 6);
        model.setQuery("РУМЫНСКАЯ ГАНТЕЛ");
        ASSERT_EQ(model.exercises().size(), 1);
        EXPECT_EQ(model.exercises().front().toMap().value("identifier").toString(), "sport-07");
        model.resetFilters();
        model.setNoFloor(true);
        EXPECT_EQ(model.exercises().size(), 3);
        model.resetFilters();
        model.setEquipmentFilter("Без инвентаря");
        EXPECT_EQ(model.exercises().size(), 4);
        model.resetFilters();
        model.selectExercise("sport-05");
        model.requestFavorite();
        EXPECT_TRUE(model.selected().value("favorite").toBool());
        EXPECT_FALSE(model.selected().value("progression").toBool());
        model.requestProgression();
        EXPECT_TRUE(model.selected().value("favorite").toBool());
        EXPECT_TRUE(model.selected().value("progression").toBool());
        model.setFavoritesOnly(true);
        EXPECT_EQ(model.exercises().size(), 1);
        model.setQuery("нет такого упражнения");
        EXPECT_TRUE(model.exercises().empty());
        model.resetFilters();
        EXPECT_EQ(model.equipmentIndex(), 0);
    }
    msa::ui::SportLibraryModel restored;
    msa::client::SportController controller{restored, path};
    controller.start();
    restored.selectExercise("sport-05");
    EXPECT_TRUE(restored.selected().value("favorite").toBool());
    EXPECT_TRUE(restored.selected().value("progression").toBool());
}
TEST(SportLibraryTest, FailedWriteNeverChangesDisplayedMark)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::ui::SportLibraryModel model;
    msa::client::SportController controller{model, path};
    controller.start();
    model.selectExercise("sport-03");
    rejectPreferenceWrites(path);
    model.requestFavorite();
    EXPECT_FALSE(model.selected().value("favorite").toBool());
    EXPECT_FALSE(model.errorText().isEmpty());
    EXPECT_EQ(model.exercises().size(), 6);
}
TEST(SportLibraryTest, BrokenDatabaseKeepsCatalogReadableAndReportsLocalFailure)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not sqlite");
    file.close();
    msa::ui::SportLibraryModel model;
    msa::client::SportController controller{model, path};
    controller.start();
    EXPECT_FALSE(model.storageReady());
    EXPECT_FALSE(model.errorText().isEmpty());
    EXPECT_EQ(model.exercises().size(), 6);
}
TEST(SportLibraryTest, DesktopLibraryAndDetailsRenderWithActualBundledContent)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::DesktopState desktop{client};
    msa::client::SportController controller{*desktop.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    desktop.sport()->openExercises();
    desktop.navigate(msa::ui::DesktopState::Page::Workouts);
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
    ASSERT_EQ(view.status(), QQuickView::Ready) << view.errors().value(0).toString().toStdString();
    view.resize(1280, 800);
    view.show();
    settle();
    ASSERT_NE(findItem(view.contentItem(), "sportExercise_sport-05"), nullptr);
    ASSERT_TRUE(saveFrame(view, "sport-desktop-library-1280x800.png"));
    desktop.sport()->selectExercise("sport-05");
    settle();
    auto* favorite = findItem(view.contentItem(), "sportFavorite");
    ASSERT_NE(favorite, nullptr);
    EXPECT_TRUE(QMetaObject::invokeMethod(favorite, "clicked"));
    EXPECT_TRUE(desktop.sport()->selected().value("favorite").toBool());
    ASSERT_TRUE(saveFrame(view, "sport-desktop-card-1280x800.png"));
    for (const auto width : {1000, 1600})
    {
        view.resize(width, width == 1000 ? 720 : 900);
        settle();
        EXPECT_TRUE(saveFrame(view, QString{"sport-desktop-card-%1.png"}.arg(width)));
    }
    EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
}
TEST(SportLibraryTest, MobileLibraryRendersAndBackKeepsFiltersAndMarks)
{
    QTemporaryDir directory;
    msa::ui::ClientModels client;
    msa::ui::MobileState mobile{client};
    msa::client::SportController controller{*mobile.sport(), directory.filePath("sport.sqlite")};
    controller.start();
    mobile.navigate(msa::ui::MobileState::Page::Sport);
    mobile.sport()->openExercises();
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
    view.show();
    for (const auto width : {320, 360, 412})
    {
        view.resize(width, 844);
        settle();
        mobile.sport()->clearSelection();
        settle();
        ASSERT_NE(findItem(view.contentItem(), "sportSearch"), nullptr);
        ASSERT_TRUE(saveFrame(view, QString{"sport-mobile-library-%1.png"}.arg(width)));
        mobile.sport()->selectExercise("sport-07");
        settle();
        ASSERT_NE(findItem(view.contentItem(), "sportBackToList"), nullptr);
        ASSERT_TRUE(saveFrame(view, QString{"sport-mobile-card-%1.png"}.arg(width)));
        EXPECT_TRUE(mobile.goBack());
        EXPECT_FALSE(mobile.sport()->hasSelection());
        EXPECT_EQ(mobile.sport()->section(), "programs");
        if (width == 360)
        {
            constexpr int kKeyboardViewportHeight = 520;
            view.resize(width, kKeyboardViewportHeight);
            settle();
            auto* search = findItem(view.contentItem(), "sportSearch");
            ASSERT_NE(search, nullptr);
            const auto position = search->mapToItem(view.contentItem(), QPointF{});
            EXPECT_LE(position.y() + search->height(), kKeyboardViewportHeight);
            EXPECT_TRUE(saveFrame(view, "sport-mobile-keyboard-360.png"));
        }
    }
    EXPECT_TRUE(mobile.goBack());
    EXPECT_EQ(mobile.sport()->section(), "overview");
    EXPECT_EQ(mobile.page(), msa::ui::MobileState::Page::Sport);
    EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
}

TEST(SportLibraryTest, OwnProgramRestoresIncompleteInputAndPublishesIndependentCopy)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    QString programIdentifier;
    {
        msa::ui::SportLibraryModel library;
        msa::client::SportController controller{library, path};
        controller.start();
        auto* journal = library.journal();
        journal->createProgram();
        auto* editor = journal->programEditor();
        ASSERT_TRUE(editor->active());
        const auto& program = editor->checkpoint().program;
        programIdentifier = QString::fromStdString(program.identifier);
        editor->setField("title", "Домашняя программа");
        editor->setField(QString::fromStdString(program.sessions.front().identifier + "/title"), "Моё занятие");
        editor->requestExercise("sport-03");
        const auto setIdentifier = editor->checkpoint().program.sessions.front().items.front().sets.front().identifier;
        editor->setField(QString::fromStdString(setIdentifier + "/reps"), "1,");
        EXPECT_TRUE(editor->saved());
        EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(editor->candidate()));
    }
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    auto* journal = library.journal();
    ASSERT_EQ(journal->drafts().size(), 1);
    journal->resumeProgram(programIdentifier);
    auto* editor = journal->programEditor();
    const auto program = editor->checkpoint().program;
    const auto setIdentifier = program.sessions.front().items.front().sets.front().identifier;
    EXPECT_EQ(msa::ui::sportForm::text(editor->checkpoint().editorText, setIdentifier + "/reps"), "1,");
    editor->setField(QString::fromStdString(setIdentifier + "/reps"), "8");
    editor->save();
    EXPECT_FALSE(editor->active());
    ASSERT_EQ(journal->programs().size(), 1);
    EXPECT_TRUE(journal->drafts().empty());
    const auto original = journal->savedPrograms().front();
    journal->copyProgram(programIdentifier);
    ASSERT_TRUE(editor->active());
    const auto copy = std::get<msa::core::SportProgramRevision>(editor->candidate());
    EXPECT_NE(copy.identifier, original.identifier);
    EXPECT_EQ(copy.sourceRevision, original.revision);
    EXPECT_NE(copy.sessions.front().items.front().sets.front().identifier, setIdentifier);
    editor->setField("title", "Отдельная копия");
    editor->save();
    ASSERT_EQ(journal->savedPrograms().size(), 2);
    const auto stored =
        std::ranges::find(journal->savedPrograms(), original.identifier, &msa::core::SportProgramRevision::identifier);
    ASSERT_NE(stored, journal->savedPrograms().end());
    EXPECT_EQ(stored->title, "Домашняя программа");
}
TEST(SportLibraryTest, ProgramWriteFailureRetainsRawInputAndPreventsLeavingUnsavedEditor)
{
    QTemporaryDir directory;
    const auto path = directory.filePath("sport.sqlite");
    msa::ui::SportLibraryModel library;
    msa::client::SportController controller{library, path};
    controller.start();
    library.journal()->createProgram();
    const auto name = QString{"sport_failure_"} + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(path);
        ASSERT_TRUE(database.open());
        QSqlQuery query{database};
        ASSERT_TRUE(query.exec(
            "CREATE TRIGGER fail_program BEFORE UPDATE ON program_drafts BEGIN SELECT RAISE(ABORT,'test'); END"));
        auto* editor = library.journal()->programEditor();
        editor->setField("title", "Не потерять");
        EXPECT_FALSE(editor->saved());
        EXPECT_TRUE(editor->status().contains("не сохранён"));
        editor->close();
        EXPECT_TRUE(editor->active());
        EXPECT_EQ(editor->checkpoint().editorText.at("title"), "Не потерять");
        ASSERT_TRUE(query.exec("DROP TRIGGER fail_program"));
        editor->retry();
        EXPECT_TRUE(editor->saved());
        editor->close();
        EXPECT_FALSE(editor->active());
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}
TEST(SportLibraryTest, ProgramEditorRendersInMobileShellAndKeepsFocusWhileSaving)
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
    ASSERT_EQ(view.status(), QQuickView::Ready);
    view.show();
    for (const auto width : {320, 360, 412})
    {
        view.resize(width, 844);
        settle();
        auto* title = findItem(view.contentItem(), "sportField_title");
        ASSERT_NE(title, nullptr);
        title->forceActiveFocus();
        ASSERT_TRUE(title->setProperty("text", "Черновик"));
        ASSERT_TRUE(QMetaObject::invokeMethod(title, "textEdited"));
        EXPECT_EQ(editor->checkpoint().editorText.at("title"), "Черновик");
        settle();
        EXPECT_EQ(findItem(view.contentItem(), "sportField_title"), title);
        EXPECT_TRUE(title->hasActiveFocus());
        EXPECT_TRUE(saveFrame(view, QString{"sport-mobile-program-%1.png"}.arg(width)));
    }
    EXPECT_TRUE(mobile.goBack());
    EXPECT_FALSE(editor->active());
    EXPECT_TRUE(warnings.empty()) << warnings.join('\n').toStdString();
}
