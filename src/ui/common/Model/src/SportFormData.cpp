#include "msa/ui/SportFormData.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace msa::ui::sportForm
{
namespace
{
[[nodiscard]] QString mass(std::optional<std::int32_t> value)
{
    if (!value)
    {
        return {};
    }
    return QString::number(*value / 100.0, 'f', 2);
}
[[nodiscard]] std::optional<std::int32_t> readMass(const QString& value)
{
    const auto parsed = core::parseSportMass(value.toStdString());
    if (const auto* error = std::get_if<core::SportRejection>(&parsed))
    {
        throw std::runtime_error{error->reason};
    }
    return std::get<std::optional<std::int32_t>>(parsed);
}
} // namespace
QString text(const Text& values, const std::string& key, const QString& fallback)
{
    const auto found = values.find(key);
    return found == values.end() ? fallback : QString::fromStdString(found->second);
}
QVariantMap field(const std::string& key, const QString& label, const QString& value)
{
    const auto name = QString::fromStdString(key);
    const bool decimal = name.endsWith("/mass") || name.endsWith("/extra") || name.endsWith("/assistance");
    const bool integerValue = name.endsWith("/reps") || name.endsWith("/quantity") || name.endsWith("/height") ||
                              name.endsWith("/distance") || name.endsWith("/replacementSets") ||
                              name.endsWith("/replacementReps") || name.endsWith("/activeSeconds") ||
                              name.endsWith("/wellbeing") || name.endsWith("/fatigue") || name.endsWith("/pain") ||
                              name == "duration";
    const auto hints = decimal ? Qt::ImhFormattedNumbersOnly : (integerValue ? Qt::ImhDigitsOnly : Qt::ImhNone);
    return {{"key", name}, {"label", label}, {"value", value}, {"inputHints", static_cast<int>(hints)}};
}
int integer(const QString& value, int minimum, int maximum, const QString& label)
{
    bool valid{};
    const auto parsed = value.toInt(&valid);
    const auto digits =
        !value.isEmpty() && std::ranges::all_of(value, [](QChar character) { return character.isDigit(); });
    if (!valid || !digits || parsed < minimum || parsed > maximum)
    {
        throw std::runtime_error{
            QString{"%1: целое число от %2 до %3."}.arg(label).arg(minimum).arg(maximum).toStdString()};
    }
    return parsed;
}
QString side(core::SportSide value)
{
    switch (value)
    {
    case core::SportSide::Both:
        return "Одновременно";
    case core::SportSide::Left:
        return "Левая сторона";
    case core::SportSide::Right:
        return "Правая сторона";
    case core::SportSide::LeftArmRightLeg:
        return "Левая рука / правая нога";
    case core::SportSide::RightArmLeftLeg:
        return "Правая рука / левая нога";
    }
    return {};
}
QString load(const core::SportLoad& value)
{
    if (const auto* dumbbells = std::get_if<core::DumbbellLoad>(&value))
    {
        return QString{"%1 кг за гантель × %2"}
            .arg(mass(dumbbells->mass).isEmpty() ? "—" : mass(dumbbells->mass))
            .arg(dumbbells->quantity);
    }
    if (const auto* barbell = std::get_if<core::BarbellLoad>(&value))
    {
        return QString{"Штанга всего: %1 кг"}.arg(mass(barbell->mass).isEmpty() ? "—" : mass(barbell->mass));
    }
    const auto& body = std::get<core::BodyweightLoad>(value);
    if (body.extra)
    {
        return "Собственный вес + " + mass(body.extra) + " кг";
    }
    if (body.assistance)
    {
        return "Противовес " + mass(body.assistance) + " кг";
    }
    return "Собственный вес";
}
QVariantList setFields(const std::string& prefix, const core::SportLoad& value, const Text& values,
                       const QString& repetitions)
{
    QVariantList fields{field(prefix + "/reps", "Повторения (1–1000)", text(values, prefix + "/reps", repetitions))};
    if (const auto* dumbbells = std::get_if<core::DumbbellLoad>(&value))
    {
        fields.append(
            field(prefix + "/mass", "Масса одной гантели, кг", text(values, prefix + "/mass", mass(dumbbells->mass))));
        fields.append(field(prefix + "/quantity", "Количество гантелей (1 или 2)",
                            text(values, prefix + "/quantity", QString::number(dumbbells->quantity))));
    }
    else if (const auto* barbell = std::get_if<core::BarbellLoad>(&value))
    {
        fields.append(field(prefix + "/mass", "Полный вес штанги с грифом, кг",
                            text(values, prefix + "/mass", mass(barbell->mass))));
    }
    else
    {
        const auto& body = std::get<core::BodyweightLoad>(value);
        fields.append(field(prefix + "/extra", "Дополнительный вес, кг (необязательно)",
                            text(values, prefix + "/extra", mass(body.extra))));
        fields.append(field(prefix + "/assistance", "Противовес, кг (вместо дополнительного)",
                            text(values, prefix + "/assistance", mass(body.assistance))));
    }
    if (std::holds_alternative<core::BodyweightLoad>(value))
    {
        for (qsizetype index = 1; index < fields.size(); ++index)
        {
            auto description = fields.at(index).toMap();
            description.insert("optionalLoad", true);
            fields[index] = description;
        }
    }
    return fields;
}
QVariantMap sideField(const std::string& identifier, core::SportSide selected,
                      const std::vector<core::SportSide>& sides, const Text& values)
{
    auto result = field(identifier + "/side", "Сторона / диагональ",
                        text(values, identifier + "/side", QString::number(static_cast<int>(selected))));
    QVariantList choices;
    for (const auto value : sides)
    {
        choices.append(QVariantMap{{"value", QString::number(static_cast<int>(value))}, {"label", side(value)}});
    }
    result.insert("choices", choices);
    return result;
}
core::SportPlannedSet readSet(const core::SportPlannedSet& base, const Text& values)
{
    auto result = base;
    result.side = static_cast<core::SportSide>(integer(
        text(values, base.identifier + "/side", QString::number(static_cast<int>(base.side))), 0, 4, "Сторона"));
    result.repetitions =
        integer(text(values, base.identifier + "/reps", QString::number(base.repetitions)), 1, 1000, "Повторения");
    if (auto* dumbbells = std::get_if<core::DumbbellLoad>(&result.load))
    {
        dumbbells->mass = readMass(text(values, base.identifier + "/mass", mass(dumbbells->mass)));
        dumbbells->quantity = integer(text(values, base.identifier + "/quantity", QString::number(dumbbells->quantity)),
                                      1, 2, "Количество гантелей");
    }
    else if (auto* barbell = std::get_if<core::BarbellLoad>(&result.load))
    {
        barbell->mass = readMass(text(values, base.identifier + "/mass", mass(barbell->mass)));
    }
    else
    {
        auto& body = std::get<core::BodyweightLoad>(result.load);
        body.extra = readMass(text(values, base.identifier + "/extra", mass(body.extra)));
        body.assistance = readMass(text(values, base.identifier + "/assistance", mass(body.assistance)));
    }
    if (const auto error = core::validateSportLoad(result.load))
    {
        throw std::runtime_error{error->reason};
    }
    return result;
}
QVariantList conditionFields(const core::SportProgramItem& item, const Text& values)
{
    const auto prefix = item.identifier;
    const auto& conditions = item.conditions;
    QVariantList result;
    if (item.exercise.requirements.stableSupport.value_or(false) || conditions.supportHeightMillimeters ||
        values.contains(prefix + "/height"))
    {
        result.append(
            field(prefix + "/height", "Высота опоры, мм (необязательно)",
                  text(values, prefix + "/height",
                       conditions.supportHeightMillimeters ? QString::number(*conditions.supportHeightMillimeters)
                                                           : QString{})));
    }
    if (item.exercise.requirements.stableSupport.value_or(false) || conditions.wallDistanceMillimeters ||
        values.contains(prefix + "/distance"))
    {
        result.append(
            field(prefix + "/distance", "Расстояние до стены, мм (необязательно)",
                  text(values, prefix + "/distance",
                       conditions.wallDistanceMillimeters ? QString::number(*conditions.wallDistanceMillimeters)
                                                          : QString{})));
    }
    result.append(field(prefix + "/amplitude", "Амплитуда / ориентир движения для сравнения",
                        text(values, prefix + "/amplitude", QString::fromStdString(conditions.amplitude))));
    result.append(field(prefix + "/conditionNote", "Другие условия выполнения",
                        text(values, prefix + "/conditionNote", QString::fromStdString(conditions.note))));
    return result;
}
core::SportExecutionConditions readConditions(const core::SportProgramItem& item, const Text& values)
{
    auto result = item.conditions;
    const auto height =
        text(values, item.identifier + "/height",
             result.supportHeightMillimeters ? QString::number(*result.supportHeightMillimeters) : QString{});
    const auto distance =
        text(values, item.identifier + "/distance",
             result.wallDistanceMillimeters ? QString::number(*result.wallDistanceMillimeters) : QString{});
    result.supportHeightMillimeters =
        height.isEmpty() ? std::nullopt
                         : std::optional{integer(height, 1, std::numeric_limits<int>::max(), "Высота опоры")};
    result.wallDistanceMillimeters =
        distance.isEmpty()
            ? std::nullopt
            : std::optional{integer(distance, 1, std::numeric_limits<int>::max(), "Расстояние до стены")};
    result.amplitude =
        text(values, item.identifier + "/amplitude", QString::fromStdString(result.amplitude)).toStdString();
    result.note = text(values, item.identifier + "/conditionNote", QString::fromStdString(result.note)).toStdString();
    // Без явно описанной амплитуды не объединяем неизвестные условия между занятиями.
    result.comparable =
        !result.amplitude.empty() && (!item.exercise.requirements.stableSupport.value_or(false) ||
                                      result.supportHeightMillimeters || result.wallDistanceMillimeters);
    return result;
}
} // namespace msa::ui::sportForm
