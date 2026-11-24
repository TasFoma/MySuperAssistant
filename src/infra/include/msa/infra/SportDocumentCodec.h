#pragma once

#include "msa/core/SportWorkoutRepository.h"

#include <QByteArray>

namespace msa::infra
{
/// @brief Версионированные локальные документы спорта; повреждение не заменяется пустым значением.
class SportDocumentCodec final
{
public:
    [[nodiscard]] static QByteArray encodeProgramCheckpoint(const core::SportProgramCheckpoint& value);
    [[nodiscard]] static core::SportProgramCheckpoint decodeProgramCheckpoint(const QByteArray& bytes);
    [[nodiscard]] static QByteArray encodeExercise(const core::SportExerciseRevision& value);
    [[nodiscard]] static core::SportExerciseRevision decodeExercise(const QByteArray& bytes);
    [[nodiscard]] static QByteArray encodeProgram(const core::SportProgramRevision& value);
    [[nodiscard]] static core::SportProgramRevision decodeProgram(const QByteArray& bytes);
    [[nodiscard]] static QByteArray encodeCheckpoint(const core::SportCheckpoint& value);
    [[nodiscard]] static core::SportCheckpoint decodeCheckpoint(const QByteArray& bytes);
    [[nodiscard]] static QByteArray encodeRevision(const core::SportWorkoutRevision& value);
    [[nodiscard]] static core::SportWorkoutRevision decodeRevision(const QByteArray& bytes);
};
} // namespace msa::infra
