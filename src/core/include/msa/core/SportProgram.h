#pragma once

#include "msa/core/SportExercise.h"

#include <cstdint>
#include <map>
#include <vector>

namespace msa::core
{
struct SportPlannedSet
{
    std::string identifier{};
    int repetitions{1};
    SportLoad load{};
    SportSide side{SportSide::Both};
};
struct SportProgramItem
{
    std::string identifier{};
    SportExerciseRevision exercise{};
    SportExecutionConditions conditions{};
    std::vector<SportPlannedSet> sets{};
};
struct SportSessionTemplate
{
    std::string identifier{};
    std::string title{};
    std::vector<SportProgramItem> items{};
};
/// @brief Собственная либо готовая программа; редакция не меняет старые снимки.
struct SportProgramRevision
{
    std::string identifier{};
    std::string revision{};
    std::string title{};
    std::string sourceRevision{};
    std::vector<SportSessionTemplate> sessions{};
};
/// @brief Неполная собственная программа и сырой ввод восстанавливаются отдельно от публикации.
struct SportProgramCheckpoint
{
    SportProgramRevision program{};
    std::map<std::string, std::string> editorText{};
    std::uint64_t sequence{0};
};
/// @brief Проверяет готовность шаблона к началу, включая подходы и условия.
[[nodiscard]] std::optional<SportRejection> validateSportSession(const SportSessionTemplate& session);
/// @brief Проверяет готовность программы; неполные черновики отклоняются.
[[nodiscard]] std::optional<SportRejection> validateSportProgram(const SportProgramRevision& program);
} // namespace msa::core
