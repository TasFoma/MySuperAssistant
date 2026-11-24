#pragma once

#include "msa/core/SportProgram.h"

namespace msa::core
{
/// @brief Локальные собственные программы с проверкой ожидаемой редакции.
class SportProgramRepository
{
public:
    virtual ~SportProgramRepository();
    /// @brief Записывает неполный редакторский черновик с проверкой номера.
    [[nodiscard]] virtual std::uint64_t checkpointProgram(const SportProgramCheckpoint& value,
                                                          std::uint64_t expectedSequence) = 0;
    [[nodiscard]] virtual std::vector<SportProgramCheckpoint> programDrafts() const = 0;
    virtual void saveProgram(const SportProgramRevision& value, const std::optional<std::string>& expectedRevision) = 0;
    [[nodiscard]] virtual std::vector<SportProgramRevision> programs() const = 0;
};
} // namespace msa::core
