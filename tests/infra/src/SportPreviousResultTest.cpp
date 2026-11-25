#include "msa/core/SportWorkoutService.h"
#include "msa/infra/SportCatalog.h"
#include "msa/ui/SportWorkoutEditor.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
[[nodiscard]] msa::core::SportWorkoutDraft draft()
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    const auto exercise = std::ranges::find(catalog, "sport-04", &msa::core::SportExerciseRevision::identifier);
    msa::core::SportProgramItem item;
    item.identifier = "item";
    item.exercise = *exercise;
    item.conditions.amplitude = "Одинаковый ориентир";
    item.conditions.comparable = true;
    item.sets = {{"planned", 8, msa::core::DumbbellLoad{125, 1}, msa::core::SportSide::Both}};
    const msa::core::SportSessionTemplate session{"session", "Тестовое занятие", {item}};

    return std::get<msa::core::SportWorkoutDraft>(msa::core::SportWorkoutService{}.start(
        std::nullopt, session, "current", std::chrono::year{2026} / 11 / 25, msa::core::SportInstant{}));
}
} // namespace
TEST(SportPreviousResultTest, ExplicitPlanCopyDoesNotPerformAndResetPreservesRawInput)
{
    msa::ui::SportWorkoutEditor editor;
    editor.openDraft({draft(), {}, 1});
    const auto identifier = QString::fromStdString(editor.checkpoint().draft.items.front().actual.front().identifier);
    EXPECT_TRUE(editor.items()
                    .front()
                    .toMap()
                    .value("sets")
                    .toList()
                    .front()
                    .toMap()
                    .value("fields")
                    .toList()
                    .front()
                    .toMap()
                    .value("value")
                    .toString()
                    .isEmpty());
    editor.usePlan("item", identifier);
    EXPECT_EQ(editor.checkpoint().editorText.at(identifier.toStdString() + "/reps"), "8");
    EXPECT_EQ(editor.checkpoint().editorText.at(identifier.toStdString() + "/mass"), "1.25");
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
    editor.recordSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Performed);
    editor.resetSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
    editor.setField(identifier + "/mass", "1,");
    const auto checkpoint = editor.checkpoint();
    msa::ui::SportWorkoutEditor restored;
    restored.openDraft(checkpoint);
    EXPECT_EQ(restored.checkpoint().editorText.at(identifier.toStdString() + "/mass"), "1,");
    EXPECT_EQ(restored.checkpoint().editorText.at(identifier.toStdString() + "/reps"), "8");
    EXPECT_EQ(restored.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
}
TEST(SportPreviousResultTest, PreviousHintTracksCurrentHeadAndRawContextWithoutCopyingValues)
{
    auto previous = draft();
    previous.identifier = "previous";
    previous.date = std::chrono::year{2026} / 11 / 24;
    auto& actual = previous.items.front().actual.front();
    actual.state = msa::core::SportSetState::Performed;
    actual.repetitions = 7;
    actual.load = msa::core::DumbbellLoad{100, 1};
    msa::core::SportWorkoutRevision head{"revision", {}, previous, false};
    msa::ui::SportWorkoutEditor editor;
    editor.openDraft({draft(), {}, 1});
    const auto identifier = QString::fromStdString(editor.checkpoint().draft.items.front().actual.front().identifier);
    editor.setPreviousResults({{head}});
    auto hint = editor.previousResults().value(identifier).toMap();
    EXPECT_EQ(hint.value("workoutId").toString(), "previous");
    EXPECT_TRUE(hint.value("text").toString().contains("7 повт."));
    EXPECT_TRUE(editor.checkpoint().editorText.empty());
    editor.setField("item/amplitude", "Другой ориентир");
    EXPECT_TRUE(editor.previousResults().value(identifier).toMap().value("workoutId").toString().isEmpty());
    editor.setField("item/amplitude", "Одинаковый ориентир");
    head.identifier = "corrected";
    head.workout.items.front().actual.front().repetitions = 9;
    editor.setPreviousResults({{head}});
    hint = editor.previousResults().value(identifier).toMap();
    EXPECT_TRUE(hint.value("text").toString().contains("9 повт."));
    EXPECT_FALSE(editor.checkpoint().draft.items.front().actual.front().repetitions.has_value());
}
TEST(SportPreviousResultTest, PendingLoadRequiresEntryOrExplicitPlanCopy)
{
    msa::ui::SportWorkoutEditor editor;
    editor.openDraft({draft(), {}, 1});
    const auto identifier = QString::fromStdString(editor.checkpoint().draft.items.front().actual.front().identifier);
    const auto fields = editor.items().front().toMap().value("sets").toList().front().toMap().value("fields").toList();
    EXPECT_TRUE(fields.at(1).toMap().value("value").toString().isEmpty());
    editor.setField(identifier + "/reps", "8");
    editor.recordSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
    editor.usePlan("item", identifier);
    editor.recordSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Performed);
}

TEST(SportPreviousResultTest, ResetPendingOrSkippedSetDoesNotAdoptPlanMass)
{
    msa::ui::SportWorkoutEditor editor;
    editor.openDraft({draft(), {}, 1});
    const auto identifier = QString::fromStdString(editor.checkpoint().draft.items.front().actual.front().identifier);
    editor.resetSet("item", identifier);
    EXPECT_TRUE(editor.checkpoint().editorText.at(identifier.toStdString() + "/mass").empty());
    editor.setField(identifier + "/reps", "8");
    editor.recordSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
    editor.skipSet("item", identifier);
    editor.resetSet("item", identifier);
    editor.recordSet("item", identifier);
    EXPECT_EQ(editor.checkpoint().draft.items.front().actual.front().state, msa::core::SportSetState::Pending);
}
TEST(SportPreviousResultTest, StorageRetryStateClearsOnlyOnAcknowledgementAndDatesAcceptBothFormats)
{
    msa::ui::SportWorkoutEditor editor;
    editor.openDraft({draft(), {}, 1});
    EXPECT_EQ(editor.workoutIdentifier(), "current");
    EXPECT_EQ(editor.fields().front().toMap().value("value").toString(), "25.11.2026");
    editor.reportStorageError("Не удалось записать");
    EXPECT_TRUE(editor.hasStorageError());
    editor.setField("date", "24.11.2026");
    EXPECT_TRUE(editor.hasStorageError());
    EXPECT_EQ(std::get<msa::core::SportWorkoutDraft>(editor.candidate()).date, std::chrono::year{2026} / 11 / 24);
    editor.setField("date", "2026-11-23");
    EXPECT_EQ(std::get<msa::core::SportWorkoutDraft>(editor.candidate()).date, std::chrono::year{2026} / 11 / 23);
    editor.acknowledge(2);
    EXPECT_TRUE(editor.saved());
    EXPECT_FALSE(editor.hasStorageError());
}
