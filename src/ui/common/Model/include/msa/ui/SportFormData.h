#pragma once

#include "msa/core/SportProgram.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <map>

namespace msa::ui::sportForm
{
using Text = std::map<std::string, std::string>;
/// @brief Возвращает сырой ввод, не заменяя пустое значение значением по умолчанию.
[[nodiscard]] QString text(const Text& values, const std::string& key, const QString& fallback = {});
/// @brief Создаёт описание текстового поля для общего QML-компонента.
[[nodiscard]] QVariantMap field(const std::string& key, const QString& label, const QString& value);
/// @brief Точно разбирает целое в диапазоне; исключение содержит пригодный для UI текст.
[[nodiscard]] int integer(const QString& text, int minimum, int maximum, const QString& label);
/// @brief Сторона в пользовательском представлении.
[[nodiscard]] QString side(core::SportSide side);
/// @brief Подпись нагрузки с сохранением смысла массы.
[[nodiscard]] QString load(const core::SportLoad& value);
/// @brief Поля подхода; пустой факт не получает рекомендованные повторения.
[[nodiscard]] QVariantList setFields(const std::string& prefix, const core::SportLoad& load, const Text& values,
                                     const QString& repetitions);
/// @brief Выбор только допустимых сторон конкретного варианта.
[[nodiscard]] QVariantMap sideField(const std::string& identifier, core::SportSide selected,
                                    const std::vector<core::SportSide>& sides, const Text& values);
/// @brief Разбирает нагрузку/сторону без назначения веса.
[[nodiscard]] core::SportPlannedSet readSet(const core::SportPlannedSet& base, const Text& values);
/// @brief Поля сопоставимости выполнения.
[[nodiscard]] QVariantList conditionFields(const core::SportProgramItem& item, const Text& values);
/// @brief Разбирает условия, существенные для отдельных серий прогресса.
[[nodiscard]] core::SportExecutionConditions readConditions(const core::SportProgramItem& item, const Text& values);
} // namespace msa::ui::sportForm
