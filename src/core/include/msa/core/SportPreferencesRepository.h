#pragma once

#include "msa/core/SportExercise.h"

#include <map>

namespace msa::core
{
/// @brief Независимые пользовательские отметки и ограничения спорта.
class SportPreferencesRepository
{
public:
    virtual ~SportPreferencesRepository();
    virtual void savePreference(const std::string& exerciseIdentifier, SportExercisePreference preference) = 0;
    [[nodiscard]] virtual std::map<std::string, SportExercisePreference> preferences() const = 0;
    virtual void saveRestrictions(const std::string& text) = 0;
    [[nodiscard]] virtual std::string restrictions() const = 0;
};
} // namespace msa::core
