#include "msa/core/SportProgramBlueprint.h"
#include "msa/core/SportWorkoutService.h"
#include "msa/infra/SportCatalog.h"
#include "msa/infra/SportProgramCatalog.h"

#include <gtest/gtest.h>

#include <QDate>
#include <QString>
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Exercise = msa::core::SportExerciseRevision;
using Blueprint = msa::core::SportProgramBlueprint;
using Program = msa::core::SportProgramRevision;
using Side = msa::core::SportSide;

[[nodiscard]] bool hasText(const std::string& text)
{
    return text.find_first_not_of(" \t\r\n") != std::string::npos;
}
[[nodiscard]] const Exercise& exerciseById(const std::vector<Exercise>& catalog, const std::string& identifier)
{
    const auto found = std::ranges::find(catalog, identifier, &Exercise::identifier);
    if (found == catalog.end())
    {
        throw std::runtime_error{"В тестируемом каталоге не найдена карточка " + identifier};
    }

    return *found;
}
[[nodiscard]] const Blueprint& programByCode(const std::vector<Blueprint>& programs, const std::string& code)
{
    const auto found = std::ranges::find(programs, code, &Blueprint::code);
    if (found == programs.end())
    {
        throw std::runtime_error{"В тестируемом каталоге не найдена заготовка " + code};
    }

    return *found;
}
[[nodiscard]] std::set<std::string> copyIdentifiers(const Program& program)
{
    std::set<std::string> result{program.identifier};
    for (const auto& session : program.sessions)
    {
        EXPECT_TRUE(result.insert(session.identifier).second);
        for (const auto& item : session.items)
        {
            EXPECT_TRUE(result.insert(item.identifier).second);
            for (const auto& planned : item.sets)
            {
                EXPECT_TRUE(result.insert(planned.identifier).second);
            }
        }
    }

    return result;
}
[[nodiscard]] std::set<std::string> blueprintIdentifiers(const Blueprint& blueprint)
{
    std::set<std::string> result{blueprint.identifier};
    for (const auto& session : blueprint.sessions)
    {
        result.insert(session.identifier);
        for (const auto& item : session.items)
        {
            result.insert(item.identifier);
        }
    }

    return result;
}
} // namespace

TEST(SportContentTest, AllTwentyTwoCardsContainReviewableInstructionsAndExistingAlternatives)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    ASSERT_EQ(catalog.size(), 22U);
    std::set<std::string> identifiers;
    for (const auto& exercise : catalog)
    {
        SCOPED_TRACE(exercise.identifier);
        EXPECT_TRUE(identifiers.insert(exercise.identifier).second);
        EXPECT_TRUE(exercise.published);
        const auto error = msa::core::validateSportExercise(exercise);
        EXPECT_FALSE(error) << (error ? error->reason : std::string{});
        EXPECT_TRUE(hasText(exercise.title));
        EXPECT_TRUE(hasText(exercise.revision));
        EXPECT_FALSE(exercise.muscles.empty());
        EXPECT_FALSE(exercise.errors.empty());
        EXPECT_FALSE(exercise.limitations.empty());
        EXPECT_TRUE(hasText(exercise.technique.brief));
        EXPECT_TRUE(hasText(exercise.technique.preparation));
        EXPECT_TRUE(hasText(exercise.technique.breathing));
        EXPECT_TRUE(hasText(exercise.technique.counting));
        ASSERT_FALSE(exercise.technique.steps.empty());
        for (const auto& step : exercise.technique.steps)
        {
            EXPECT_TRUE(hasText(step));
        }
        for (const auto& values : {exercise.muscles, exercise.equipment, exercise.errors, exercise.limitations})
        {
            for (const auto& value : values)
            {
                EXPECT_TRUE(hasText(value));
            }
        }
        EXPECT_TRUE(exercise.requirements.palmSupport.has_value());
        EXPECT_TRUE(exercise.requirements.floorTransition.has_value());
        EXPECT_TRUE(exercise.requirements.kneeSupport.has_value());
        EXPECT_TRUE(exercise.requirements.stableSupport.has_value());
        if (exercise.requirements.stableSupport.value_or(false))
        {
            EXPECT_TRUE(hasText(exercise.requirements.supportType));
        }
        if (exercise.loadKind == 1)
        {
            ASSERT_TRUE(exercise.requirements.dumbbellQuantity);
            EXPECT_TRUE(*exercise.requirements.dumbbellQuantity == 1 || *exercise.requirements.dumbbellQuantity == 2);
        }
        else
        {
            EXPECT_FALSE(exercise.requirements.dumbbellQuantity);
        }
        ASSERT_FALSE(exercise.sources.empty());
        for (const auto& source : exercise.sources)
        {
            EXPECT_TRUE(hasText(source.title));
            EXPECT_TRUE(source.url.starts_with("https://"));
            const auto date = QDate::fromString(QString::fromStdString(source.checkedDate), Qt::ISODate);
            EXPECT_TRUE(date.isValid());
            EXPECT_EQ(date.toString(Qt::ISODate).toStdString(), source.checkedDate);
        }
        std::set<std::string> alternatives;
        for (const auto& identifier : exercise.alternatives)
        {
            EXPECT_NE(identifier, exercise.identifier);
            EXPECT_TRUE(alternatives.insert(identifier).second);
            EXPECT_TRUE(exerciseById(catalog, identifier).published);
        }
    }
}

TEST(SportContentTest, SupportAndSideRequirementsDistinguishConcreteVariants)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    struct Requirements
    {
        std::string identifier{};
        bool palms{};
        bool floor{};
        bool knees{};
        bool stable{};
        std::string equipment{};
        std::vector<Side> sides{};
    };
    const std::vector<Requirements> cases{
        {"sport-01", false, false, false, true, "Устойчивая опора", {Side::Both}},
        {"sport-02", true, false, false, true, "Устойчивая опора", {Side::Both}},
        {"sport-09", true, false, false, true, "Стена", {Side::Both}},
        {"sport-13", true, false, true, true, "Скамья", {Side::Left, Side::Right}},
        {"sport-16", false, false, false, false, "Гантели", {Side::Both}},
        {"sport-18", true, false, false, true, "Устойчивая опора", {Side::Both}},
        {"sport-19", true, false, false, true, "Устойчивая опора", {Side::Left, Side::Right}},
        {"sport-20", true, true, true, false, {}, {Side::LeftArmRightLeg, Side::RightArmLeftLeg}},
        {"sport-21", false, true, false, false, {}, {Side::LeftArmRightLeg, Side::RightArmLeftLeg}},
        {"sport-22", true, false, false, true, "Устойчивая опора", {Side::Left, Side::Right}}};
    for (const auto& expected : cases)
    {
        SCOPED_TRACE(expected.identifier);
        const auto& exercise = exerciseById(catalog, expected.identifier);
        EXPECT_EQ(exercise.requirements.palmSupport, expected.palms);
        EXPECT_EQ(exercise.requirements.floorTransition, expected.floor);
        EXPECT_EQ(exercise.requirements.kneeSupport, expected.knees);
        EXPECT_EQ(exercise.requirements.stableSupport, expected.stable);
        EXPECT_EQ(exercise.sides, expected.sides);
        if (expected.equipment.empty())
        {
            EXPECT_TRUE(exercise.equipment.empty());
        }
        else
        {
            EXPECT_NE(std::ranges::find(exercise.equipment, expected.equipment), exercise.equipment.end());
        }
    }
    EXPECT_EQ(exerciseById(catalog, "sport-04").requirements.dumbbellQuantity, 1);
    EXPECT_EQ(exerciseById(catalog, "sport-13").requirements.dumbbellQuantity, 1);
    EXPECT_EQ(exerciseById(catalog, "sport-14").requirements.dumbbellQuantity, 2);
    EXPECT_EQ(exerciseById(catalog, "sport-16").requirements.dumbbellQuantity, 2);
    EXPECT_EQ(exerciseById(catalog, "sport-13").loadKind, 1);
    EXPECT_EQ(exerciseById(catalog, "sport-16").loadKind, 1);
    EXPECT_EQ(exerciseById(catalog, "sport-19").loadKind, 0);
    EXPECT_EQ(exerciseById(catalog, "sport-20").loadKind, 0);
    EXPECT_EQ(exerciseById(catalog, "sport-21").loadKind, 0);
}

TEST(SportContentTest, P1AndP3ProvideCompleteCompositionsWithResolvableManualAlternatives)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    const auto programs = msa::infra::loadBundledSportProgramCatalog(catalog);
    ASSERT_EQ(programs.size(), 2U);
    for (const auto& program : programs)
    {
        SCOPED_TRACE(program.code);
        EXPECT_FALSE(msa::core::validateSportProgramBlueprint(program, catalog));
        EXPECT_TRUE(hasText(program.title));
        EXPECT_TRUE(hasText(program.description));
        EXPECT_TRUE(hasText(program.authorship));
        EXPECT_FALSE(program.equipment.empty());
        EXPECT_FALSE(program.conditions.empty());
        ASSERT_EQ(program.sessions.size(), 2U);
        for (const auto& session : program.sessions)
        {
            EXPECT_TRUE(hasText(session.title));
            EXPECT_TRUE(hasText(session.description));
            ASSERT_FALSE(session.items.empty());
            for (const auto& item : session.items)
            {
                SCOPED_TRACE(item.identifier);
                EXPECT_TRUE(hasText(item.instructions));
                const auto& exercise = exerciseById(catalog, item.exerciseIdentifier);
                EXPECT_TRUE(exercise.published);
                std::set<std::string> alternatives;
                for (const auto& alternative : item.alternatives)
                {
                    EXPECT_NE(alternative.exerciseIdentifier, item.exerciseIdentifier);
                    EXPECT_TRUE(hasText(alternative.description));
                    EXPECT_TRUE(alternatives.insert(alternative.exerciseIdentifier).second);
                    EXPECT_TRUE(exerciseById(catalog, alternative.exerciseIdentifier).published);
                }
            }
        }
    }
    const auto& support = programByCode(programs, "P1");
    for (const auto& session : support.sessions)
    {
        for (const auto& item : session.items)
        {
            const auto& exercise = exerciseById(catalog, item.exerciseIdentifier);
            EXPECT_EQ(exercise.loadKind, 0);
            EXPECT_EQ(exercise.requirements.floorTransition, false);
        }
    }
    const auto& dumbbells = programByCode(programs, "P3");
    for (const auto& session : dumbbells.sessions)
    {
        for (const auto& item : session.items)
        {
            const auto& exercise = exerciseById(catalog, item.exerciseIdentifier);
            EXPECT_NE(exercise.loadKind, 2);
            EXPECT_NE(exercise.identifier, "sport-13");
            EXPECT_EQ(std::ranges::find(exercise.equipment, std::string{"Скамья"}), exercise.equipment.end());
        }
    }
}

TEST(SportContentTest, BlueprintCopiesRemainIndependentIncompletePlansWithoutAssignedDose)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    const auto blueprints = msa::infra::loadBundledSportProgramCatalog(catalog);
    for (const auto& blueprint : blueprints)
    {
        SCOPED_TRACE(blueprint.code);
        const auto firstResult = msa::core::makeSportProgramBlueprintCopy(blueprint, catalog, "own-one", "one-r1");
        const auto secondResult = msa::core::makeSportProgramBlueprintCopy(blueprint, catalog, "own-two", "two-r1");
        ASSERT_TRUE(std::holds_alternative<Program>(firstResult));
        ASSERT_TRUE(std::holds_alternative<Program>(secondResult));
        auto first = std::get<Program>(firstResult);
        const auto second = std::get<Program>(secondResult);
        EXPECT_EQ(first.sourceRevision, blueprint.revision);
        EXPECT_NE(first.identifier, blueprint.identifier);
        EXPECT_NE(first.revision, blueprint.revision);
        EXPECT_TRUE(msa::core::validateSportProgram(first));
        EXPECT_TRUE(msa::core::validateSportProgram(second));
        ASSERT_EQ(first.sessions.size(), blueprint.sessions.size());
        const auto firstIdentifiers = copyIdentifiers(first);
        const auto secondIdentifiers = copyIdentifiers(second);
        const auto sourceIdentifiers = blueprintIdentifiers(blueprint);
        for (const auto& identifier : firstIdentifiers)
        {
            EXPECT_FALSE(secondIdentifiers.contains(identifier));
            EXPECT_FALSE(sourceIdentifiers.contains(identifier));
        }
        for (std::size_t sessionIndex = 0; sessionIndex < first.sessions.size(); ++sessionIndex)
        {
            const auto& session = first.sessions.at(sessionIndex);
            const auto& source = blueprint.sessions.at(sessionIndex);
            ASSERT_EQ(session.items.size(), source.items.size());
            EXPECT_EQ(session.title, source.title);
            EXPECT_TRUE(msa::core::validateSportSession(session));
            for (std::size_t itemIndex = 0; itemIndex < session.items.size(); ++itemIndex)
            {
                const auto& item = session.items.at(itemIndex);
                const auto& definition = exerciseById(catalog, source.items.at(itemIndex).exerciseIdentifier);
                EXPECT_EQ(item.exercise.identifier, definition.identifier);
                EXPECT_EQ(item.exercise.revision, definition.revision);
                EXPECT_TRUE(item.sets.empty());
                EXPECT_EQ(item.conditions, msa::core::SportExecutionConditions{});
            }
        }
        const auto start =
            msa::core::SportWorkoutService{}.start(std::nullopt, first.sessions.front(), "not-ready",
                                                   std::chrono::year{2026} / 11 / 24, msa::core::SportInstant{});
        EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(start));
        const auto exerciseIdentifier = first.sessions.front().items.front().exercise.identifier;
        const auto originalBrief = exerciseById(catalog, exerciseIdentifier).technique.brief;
        const auto blueprintItem = blueprint.sessions.front().items.front().exerciseIdentifier;
        first.title = "Изменённая собственная копия";
        first.sessions.front().items.front().exercise.technique.brief = "Текст только в копии";
        first.sessions.front().items.clear();
        EXPECT_EQ(exerciseById(catalog, exerciseIdentifier).technique.brief, originalBrief);
        EXPECT_EQ(blueprint.sessions.front().items.front().exerciseIdentifier, blueprintItem);
        EXPECT_EQ(second.sessions.front().items.front().exercise.technique.brief, originalBrief);
        EXPECT_NE(first.title, second.title);
    }
}

TEST(SportContentTest, UnpublishedOrIncompleteSelectedCardPreventsCreatingBlueprintCopy)
{
    const auto originalCatalog = msa::infra::loadBundledSportCatalog();
    const auto blueprints = msa::infra::loadBundledSportProgramCatalog(originalCatalog);
    ASSERT_FALSE(blueprints.empty());
    const auto& blueprint = blueprints.front();
    const auto identifier = blueprint.sessions.front().items.front().exerciseIdentifier;
    auto catalog = originalCatalog;
    const auto selected = std::ranges::find(catalog, identifier, &Exercise::identifier);
    ASSERT_NE(selected, catalog.end());
    selected->published = false;
    EXPECT_FALSE(msa::core::validateSportProgramBlueprint(blueprint, catalog));
    const auto unpublished = msa::core::makeSportProgramBlueprintCopy(blueprint, catalog, "own", "own-r1");
    ASSERT_TRUE(std::holds_alternative<msa::core::SportRejection>(unpublished));
    EXPECT_EQ(std::get<msa::core::SportRejection>(unpublished).code, "unpublished");
    EXPECT_TRUE(exerciseById(originalCatalog, identifier).published);
    selected->published = true;
    selected->technique.brief.clear();
    const auto incomplete = msa::core::makeSportProgramBlueprintCopy(blueprint, catalog, "own", "own-r1");
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(incomplete));
    EXPECT_FALSE(exerciseById(originalCatalog, identifier).technique.brief.empty());
}

TEST(SportContentTest, InvalidReferencesAndSourceIdentitiesAreRejectedBeforeCopy)
{
    const auto catalog = msa::infra::loadBundledSportCatalog();
    const auto blueprints = msa::infra::loadBundledSportProgramCatalog(catalog);
    ASSERT_FALSE(blueprints.empty());
    auto blueprint = blueprints.front();
    const auto unchanged = blueprint.sessions.front().items.front().exerciseIdentifier;
    blueprint.sessions.front().items.front().exerciseIdentifier = "unknown-exercise";
    EXPECT_TRUE(msa::core::validateSportProgramBlueprint(blueprint, catalog));
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(
        msa::core::makeSportProgramBlueprintCopy(blueprint, catalog, "own", "own-r1")));
    blueprint = blueprints.front();
    blueprint.sessions.front().items.front().alternatives = {{unchanged, "Не является альтернативой самому себе"}};
    EXPECT_TRUE(msa::core::validateSportProgramBlueprint(blueprint, catalog));
    blueprint = blueprints.front();
    blueprint.sessions.front().items.front().alternatives = {{"unknown-alternative", "Неизвестная карточка"}};
    EXPECT_TRUE(msa::core::validateSportProgramBlueprint(blueprint, catalog));
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(msa::core::makeSportProgramBlueprintCopy(
        blueprints.front(), catalog, blueprints.front().identifier, "own-r1")));
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(
        msa::core::makeSportProgramBlueprintCopy(blueprints.front(), catalog, "own", blueprints.front().revision)));
    EXPECT_TRUE(std::holds_alternative<msa::core::SportRejection>(
        msa::core::makeSportProgramBlueprintCopy(blueprints.front(), catalog, "", "own-r1")));
    EXPECT_EQ(blueprints.front().sessions.front().items.front().exerciseIdentifier, unchanged);
}
