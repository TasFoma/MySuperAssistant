#include "msa/infra/SportDocumentCodec.h"

#include "msa/core/SportWorkoutService.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace msa::infra
{
namespace
{
constexpr qsizetype kMaximumDocumentBytes = 8 * 1024 * 1024;
constexpr qsizetype kMaximumTextCharacters = 1024 * 1024;
constexpr qsizetype kMaximumArrayItems = 10000;
void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error{"Некорректный или неподдерживаемый документ спорта."};
    }
}
[[nodiscard]] QJsonObject object(const QJsonValue& value, const QStringList& keys)
{
    require(value.isObject());
    const auto result = value.toObject();
    require(result.size() == keys.size());
    for (const auto& key : keys)
    {
        require(result.contains(key));
    }

    return result;
}
[[nodiscard]] std::string textValue(const QJsonValue& value)
{
    require(value.isString() && value.toString().size() <= kMaximumTextCharacters);

    return value.toString().toStdString();
}
[[nodiscard]] int integer(const QJsonValue& value)
{
    require(value.isDouble());
    const auto number = value.toDouble();
    require(std::isfinite(number) && std::floor(number) == number && number >= std::numeric_limits<int>::min() &&
            number <= std::numeric_limits<int>::max());

    return static_cast<int>(number);
}
[[nodiscard]] bool boolean(const QJsonValue& value)
{
    require(value.isBool());

    return value.toBool();
}
[[nodiscard]] std::int64_t integerText(const QJsonValue& value)
{
    require(value.isString());
    bool valid{};
    const auto result = value.toString().toLongLong(&valid);
    require(valid && QString::number(result) == value.toString());

    return result;
}
[[nodiscard]] std::uint64_t unsignedText(const QJsonValue& value)
{
    require(value.isString());
    bool valid{};
    const auto result = value.toString().toULongLong(&valid);
    require(valid && QString::number(result) == value.toString());

    return result;
}
[[nodiscard]] core::SportSide sideValue(const QJsonValue& value)
{
    const auto number = integer(value);
    require(number >= 0 && number <= 4);

    return static_cast<core::SportSide>(number);
}
[[nodiscard]] core::SportSetState stateValue(const QJsonValue& value)
{
    const auto number = integer(value);
    require(number >= 0 && number <= 2);

    return static_cast<core::SportSetState>(number);
}
[[nodiscard]] core::SportAbsenceDecision decisionValue(const QJsonValue& value)
{
    const auto number = integer(value);
    require(number >= 0 && number <= 2);

    return static_cast<core::SportAbsenceDecision>(number);
}
[[nodiscard]] core::SportInstant instantValue(const QJsonValue& value)
{
    return core::SportInstant{core::SportDuration{integerText(value)}};
}
[[nodiscard]] core::SportDuration durationValue(const QJsonValue& value)
{
    const auto count = integerText(value);
    require(count >= 0);

    return core::SportDuration{count};
}
[[nodiscard]] std::chrono::year_month_day dateValue(const QJsonValue& value)
{
    const auto date = QDate::fromString(QString::fromStdString(textValue(value)), Qt::ISODate);
    require(date.isValid());

    return std::chrono::year{date.year()} / static_cast<unsigned>(date.month()) / static_cast<unsigned>(date.day());
}
[[nodiscard]] QJsonValue encodeText(const std::string& value)
{
    return QString::fromStdString(value);
}
[[nodiscard]] QJsonValue encodeSportLoad(const core::SportLoad& value);
[[nodiscard]] core::SportLoad decodeSportLoad(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeDumbbellLoad(const core::DumbbellLoad& value);
[[nodiscard]] core::DumbbellLoad decodeDumbbellLoad(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeBarbellLoad(const core::BarbellLoad& value);
[[nodiscard]] core::BarbellLoad decodeBarbellLoad(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeBodyweightLoad(const core::BodyweightLoad& value);
[[nodiscard]] core::BodyweightLoad decodeBodyweightLoad(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportExecutionRequirements(const core::SportExecutionRequirements& value);
[[nodiscard]] core::SportExecutionRequirements decodeSportExecutionRequirements(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportExecutionConditions(const core::SportExecutionConditions& value);
[[nodiscard]] core::SportExecutionConditions decodeSportExecutionConditions(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportTechnique(const core::SportTechnique& value);
[[nodiscard]] core::SportTechnique decodeSportTechnique(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportSource(const core::SportSource& value);
[[nodiscard]] core::SportSource decodeSportSource(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportExerciseRevision(const core::SportExerciseRevision& value);
[[nodiscard]] core::SportExerciseRevision decodeSportExerciseRevision(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportPlannedSet(const core::SportPlannedSet& value);
[[nodiscard]] core::SportPlannedSet decodeSportPlannedSet(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportProgramItem(const core::SportProgramItem& value);
[[nodiscard]] core::SportProgramItem decodeSportProgramItem(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportSessionTemplate(const core::SportSessionTemplate& value);
[[nodiscard]] core::SportSessionTemplate decodeSportSessionTemplate(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportProgramRevision(const core::SportProgramRevision& value);
[[nodiscard]] core::SportProgramRevision decodeSportProgramRevision(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportWellbeing(const core::SportWellbeing& value);
[[nodiscard]] core::SportWellbeing decodeSportWellbeing(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportTimerSegment(const core::SportTimerSegment& value);
[[nodiscard]] core::SportTimerSegment decodeSportTimerSegment(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportAbsenceInterval(const core::SportAbsenceInterval& value);
[[nodiscard]] core::SportAbsenceInterval decodeSportAbsenceInterval(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportTimerState(const core::SportTimerState& value);
[[nodiscard]] core::SportTimerState decodeSportTimerState(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportActualSet(const core::SportActualSet& value);
[[nodiscard]] core::SportActualSet decodeSportActualSet(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportWorkoutItem(const core::SportWorkoutItem& value);
[[nodiscard]] core::SportWorkoutItem decodeSportWorkoutItem(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportWorkoutDraft(const core::SportWorkoutDraft& value);
[[nodiscard]] core::SportWorkoutDraft decodeSportWorkoutDraft(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportWorkoutRevision(const core::SportWorkoutRevision& value);
[[nodiscard]] core::SportWorkoutRevision decodeSportWorkoutRevision(const QJsonValue& value);
[[nodiscard]] QJsonValue encodeSportCheckpoint(const core::SportCheckpoint& value);
[[nodiscard]] core::SportCheckpoint decodeSportCheckpoint(const QJsonValue& value);
template <typename ItemType, typename DecoderType>
[[nodiscard]] std::vector<ItemType> arrayValue(const QJsonValue& value, DecoderType decoder)
{
    require(value.isArray() && value.toArray().size() <= kMaximumArrayItems);
    std::vector<ItemType> result;
    for (const auto& item : value.toArray())
    {
        result.push_back(decoder(item));
    }

    return result;
}
template <typename ItemType, typename EncoderType>
[[nodiscard]] QJsonArray encodeArray(const std::vector<ItemType>& values, EncoderType encoder)
{
    QJsonArray result;
    for (const auto& value : values)
    {
        result.append(encoder(value));
    }

    return result;
}
template <typename ItemType, typename DecoderType>
[[nodiscard]] std::optional<ItemType> optionalValue(const QJsonValue& value, DecoderType decoder)
{
    if (value.isNull())
    {
        return std::nullopt;
    }

    return decoder(value);
}
[[nodiscard]] std::map<std::string, std::string> mapValue(const QJsonValue& value)
{
    require(value.isObject());
    std::map<std::string, std::string> result;
    const auto values = value.toObject();
    for (auto iterator = values.begin(); iterator != values.end(); ++iterator)
    {
        result.emplace(iterator.key().toStdString(), textValue(iterator.value()));
    }

    return result;
}
[[nodiscard]] QJsonValue encodeMap(const std::map<std::string, std::string>& value)
{
    QJsonObject result;
    for (const auto& [key, item] : value)
    {
        result.insert(QString::fromStdString(key), encodeText(item));
    }

    return result;
}
[[nodiscard]] QJsonValue encodeSportLoad(const core::SportLoad& value)
{
    const auto data = std::visit(
        [](const auto& load) -> QJsonValue
        {
            using LoadType = std::decay_t<decltype(load)>;
            if constexpr (std::is_same_v<LoadType, core::BodyweightLoad>)
            {
                return encodeBodyweightLoad(load);
            }
            else if constexpr (std::is_same_v<LoadType, core::DumbbellLoad>)
            {
                return encodeDumbbellLoad(load);
            }
            else
            {
                return encodeBarbellLoad(load);
            }
        },
        value);

    return QJsonObject{{"kind", static_cast<int>(value.index())}, {"data", data}};
}
[[nodiscard]] core::SportLoad decodeSportLoad(const QJsonValue& value)
{
    const auto fields = object(value, {"kind", "data"});
    const auto kind = integer(fields.value("kind"));
    core::SportLoad result;
    if (kind == 0)
    {
        result = decodeBodyweightLoad(fields.value("data"));
    }
    else if (kind == 1)
    {
        result = decodeDumbbellLoad(fields.value("data"));
    }
    else if (kind == 2)
    {
        result = decodeBarbellLoad(fields.value("data"));
    }
    else
    {
        require(false);
    }
    require(!core::validateSportLoad(result));

    return result;
}
QJsonValue encodeDumbbellLoad(const core::DumbbellLoad& value)
{
    return QJsonObject{{"mass", (value.mass ? QJsonValue{*value.mass} : QJsonValue{})}, {"quantity", value.quantity}};
}
core::DumbbellLoad decodeDumbbellLoad(const QJsonValue& value)
{
    const auto fields = object(value, {"mass", "quantity"});
    core::DumbbellLoad result;
    result.mass = optionalValue<int>(fields.value("mass"), integer);
    result.quantity = integer(fields.value("quantity"));

    return result;
}
QJsonValue encodeBarbellLoad(const core::BarbellLoad& value)
{
    return QJsonObject{{"mass", (value.mass ? QJsonValue{*value.mass} : QJsonValue{})}};
}
core::BarbellLoad decodeBarbellLoad(const QJsonValue& value)
{
    const auto fields = object(value, {"mass"});
    core::BarbellLoad result;
    result.mass = optionalValue<int>(fields.value("mass"), integer);

    return result;
}
QJsonValue encodeBodyweightLoad(const core::BodyweightLoad& value)
{
    return QJsonObject{{"extra", (value.extra ? QJsonValue{*value.extra} : QJsonValue{})},
                       {"assistance", (value.assistance ? QJsonValue{*value.assistance} : QJsonValue{})}};
}
core::BodyweightLoad decodeBodyweightLoad(const QJsonValue& value)
{
    const auto fields = object(value, {"extra", "assistance"});
    core::BodyweightLoad result;
    result.extra = optionalValue<int>(fields.value("extra"), integer);
    result.assistance = optionalValue<int>(fields.value("assistance"), integer);

    return result;
}
QJsonValue encodeSportExecutionRequirements(const core::SportExecutionRequirements& value)
{
    return QJsonObject{{"palmSupport", (value.palmSupport ? QJsonValue{*value.palmSupport} : QJsonValue{})},
                       {"floorTransition", (value.floorTransition ? QJsonValue{*value.floorTransition} : QJsonValue{})},
                       {"kneeSupport", (value.kneeSupport ? QJsonValue{*value.kneeSupport} : QJsonValue{})},
                       {"stableSupport", (value.stableSupport ? QJsonValue{*value.stableSupport} : QJsonValue{})},
                       {"supportType", encodeText(value.supportType)}};
}
core::SportExecutionRequirements decodeSportExecutionRequirements(const QJsonValue& value)
{
    const auto fields =
        object(value, {"palmSupport", "floorTransition", "kneeSupport", "stableSupport", "supportType"});
    core::SportExecutionRequirements result;
    result.palmSupport = optionalValue<bool>(fields.value("palmSupport"), boolean);
    result.floorTransition = optionalValue<bool>(fields.value("floorTransition"), boolean);
    result.kneeSupport = optionalValue<bool>(fields.value("kneeSupport"), boolean);
    result.stableSupport = optionalValue<bool>(fields.value("stableSupport"), boolean);
    result.supportType = textValue(fields.value("supportType"));

    return result;
}
QJsonValue encodeSportExecutionConditions(const core::SportExecutionConditions& value)
{
    return QJsonObject{{"supportHeightMillimeters",
                        (value.supportHeightMillimeters ? QJsonValue{*value.supportHeightMillimeters} : QJsonValue{})},
                       {"wallDistanceMillimeters",
                        (value.wallDistanceMillimeters ? QJsonValue{*value.wallDistanceMillimeters} : QJsonValue{})},
                       {"amplitude", encodeText(value.amplitude)},
                       {"note", encodeText(value.note)},
                       {"comparable", value.comparable}};
}
core::SportExecutionConditions decodeSportExecutionConditions(const QJsonValue& value)
{
    const auto fields =
        object(value, {"supportHeightMillimeters", "wallDistanceMillimeters", "amplitude", "note", "comparable"});
    core::SportExecutionConditions result;
    result.supportHeightMillimeters = optionalValue<int>(fields.value("supportHeightMillimeters"), integer);
    result.wallDistanceMillimeters = optionalValue<int>(fields.value("wallDistanceMillimeters"), integer);
    result.amplitude = textValue(fields.value("amplitude"));
    result.note = textValue(fields.value("note"));
    result.comparable = boolean(fields.value("comparable"));

    return result;
}
QJsonValue encodeSportTechnique(const core::SportTechnique& value)
{
    return QJsonObject{{"brief", encodeText(value.brief)},
                       {"preparation", encodeText(value.preparation)},
                       {"steps", encodeArray(value.steps, [](const auto& item) { return encodeText(item); })},
                       {"breathing", encodeText(value.breathing)},
                       {"counting", encodeText(value.counting)}};
}
core::SportTechnique decodeSportTechnique(const QJsonValue& value)
{
    const auto fields = object(value, {"brief", "preparation", "steps", "breathing", "counting"});
    core::SportTechnique result;
    result.brief = textValue(fields.value("brief"));
    result.preparation = textValue(fields.value("preparation"));
    result.steps = arrayValue<std::string>(fields.value("steps"), textValue);
    result.breathing = textValue(fields.value("breathing"));
    result.counting = textValue(fields.value("counting"));

    return result;
}
QJsonValue encodeSportSource(const core::SportSource& value)
{
    return QJsonObject{{"title", encodeText(value.title)},
                       {"url", encodeText(value.url)},
                       {"checkedDate", encodeText(value.checkedDate)}};
}
core::SportSource decodeSportSource(const QJsonValue& value)
{
    const auto fields = object(value, {"title", "url", "checkedDate"});
    core::SportSource result;
    result.title = textValue(fields.value("title"));
    result.url = textValue(fields.value("url"));
    result.checkedDate = textValue(fields.value("checkedDate"));

    return result;
}
QJsonValue encodeSportExerciseRevision(const core::SportExerciseRevision& value)
{
    return QJsonObject{
        {"identifier", encodeText(value.identifier)},
        {"revision", encodeText(value.revision)},
        {"title", encodeText(value.title)},
        {"aliases", encodeArray(value.aliases, [](const auto& item) { return encodeText(item); })},
        {"muscles", encodeArray(value.muscles, [](const auto& item) { return encodeText(item); })},
        {"equipment", encodeArray(value.equipment, [](const auto& item) { return encodeText(item); })},
        {"errors", encodeArray(value.errors, [](const auto& item) { return encodeText(item); })},
        {"limitations", encodeArray(value.limitations, [](const auto& item) { return encodeText(item); })},
        {"alternatives", encodeArray(value.alternatives, [](const auto& item) { return encodeText(item); })},
        {"technique", encodeSportTechnique(value.technique)},
        {"requirements", encodeSportExecutionRequirements(value.requirements)},
        {"sources", encodeArray(value.sources, [](const auto& item) { return encodeSportSource(item); })},
        {"sides", encodeArray(value.sides, [](const auto& item) { return static_cast<int>(item); })},
        {"loadKind", value.loadKind},
        {"published", value.published}};
}
core::SportExerciseRevision decodeSportExerciseRevision(const QJsonValue& value)
{
    const auto fields =
        object(value, {"identifier", "revision", "title", "aliases", "muscles", "equipment", "errors", "limitations",
                       "alternatives", "technique", "requirements", "sources", "sides", "loadKind", "published"});
    core::SportExerciseRevision result;
    result.identifier = textValue(fields.value("identifier"));
    result.revision = textValue(fields.value("revision"));
    result.title = textValue(fields.value("title"));
    result.aliases = arrayValue<std::string>(fields.value("aliases"), textValue);
    result.muscles = arrayValue<std::string>(fields.value("muscles"), textValue);
    result.equipment = arrayValue<std::string>(fields.value("equipment"), textValue);
    result.errors = arrayValue<std::string>(fields.value("errors"), textValue);
    result.limitations = arrayValue<std::string>(fields.value("limitations"), textValue);
    result.alternatives = arrayValue<std::string>(fields.value("alternatives"), textValue);
    result.technique = decodeSportTechnique(fields.value("technique"));
    result.requirements = decodeSportExecutionRequirements(fields.value("requirements"));
    result.sources = arrayValue<core::SportSource>(fields.value("sources"), decodeSportSource);
    result.sides = arrayValue<core::SportSide>(fields.value("sides"), sideValue);
    result.loadKind = integer(fields.value("loadKind"));
    result.published = boolean(fields.value("published"));

    return result;
}
QJsonValue encodeSportPlannedSet(const core::SportPlannedSet& value)
{
    return QJsonObject{{"identifier", encodeText(value.identifier)},
                       {"repetitions", value.repetitions},
                       {"load", encodeSportLoad(value.load)},
                       {"side", static_cast<int>(value.side)}};
}
core::SportPlannedSet decodeSportPlannedSet(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "repetitions", "load", "side"});
    core::SportPlannedSet result;
    result.identifier = textValue(fields.value("identifier"));
    result.repetitions = integer(fields.value("repetitions"));
    result.load = decodeSportLoad(fields.value("load"));
    result.side = sideValue(fields.value("side"));

    return result;
}
QJsonValue encodeSportProgramItem(const core::SportProgramItem& value)
{
    return QJsonObject{{"identifier", encodeText(value.identifier)},
                       {"exercise", encodeSportExerciseRevision(value.exercise)},
                       {"conditions", encodeSportExecutionConditions(value.conditions)},
                       {"sets", encodeArray(value.sets, [](const auto& item) { return encodeSportPlannedSet(item); })}};
}
core::SportProgramItem decodeSportProgramItem(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "exercise", "conditions", "sets"});
    core::SportProgramItem result;
    result.identifier = textValue(fields.value("identifier"));
    result.exercise = decodeSportExerciseRevision(fields.value("exercise"));
    result.conditions = decodeSportExecutionConditions(fields.value("conditions"));
    result.sets = arrayValue<core::SportPlannedSet>(fields.value("sets"), decodeSportPlannedSet);

    return result;
}
QJsonValue encodeSportSessionTemplate(const core::SportSessionTemplate& value)
{
    return QJsonObject{
        {"identifier", encodeText(value.identifier)},
        {"title", encodeText(value.title)},
        {"items", encodeArray(value.items, [](const auto& item) { return encodeSportProgramItem(item); })}};
}
core::SportSessionTemplate decodeSportSessionTemplate(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "title", "items"});
    core::SportSessionTemplate result;
    result.identifier = textValue(fields.value("identifier"));
    result.title = textValue(fields.value("title"));
    result.items = arrayValue<core::SportProgramItem>(fields.value("items"), decodeSportProgramItem);

    return result;
}
QJsonValue encodeSportProgramRevision(const core::SportProgramRevision& value)
{
    return QJsonObject{
        {"identifier", encodeText(value.identifier)},
        {"revision", encodeText(value.revision)},
        {"title", encodeText(value.title)},
        {"sourceRevision", encodeText(value.sourceRevision)},
        {"sessions", encodeArray(value.sessions, [](const auto& item) { return encodeSportSessionTemplate(item); })}};
}
core::SportProgramRevision decodeSportProgramRevision(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "revision", "title", "sourceRevision", "sessions"});
    core::SportProgramRevision result;
    result.identifier = textValue(fields.value("identifier"));
    result.revision = textValue(fields.value("revision"));
    result.title = textValue(fields.value("title"));
    result.sourceRevision = textValue(fields.value("sourceRevision"));
    result.sessions = arrayValue<core::SportSessionTemplate>(fields.value("sessions"), decodeSportSessionTemplate);

    return result;
}
QJsonValue encodeSportWellbeing(const core::SportWellbeing& value)
{
    return QJsonObject{{"wellbeing", (value.wellbeing ? QJsonValue{*value.wellbeing} : QJsonValue{})},
                       {"fatigue", (value.fatigue ? QJsonValue{*value.fatigue} : QJsonValue{})},
                       {"pain", (value.pain ? QJsonValue{*value.pain} : QJsonValue{})},
                       {"areas", encodeArray(value.areas, [](const auto& item) { return encodeText(item); })},
                       {"otherArea", encodeText(value.otherArea)},
                       {"newPain", (value.newPain ? QJsonValue{*value.newPain} : QJsonValue{})},
                       {"comment", encodeText(value.comment)}};
}
core::SportWellbeing decodeSportWellbeing(const QJsonValue& value)
{
    const auto fields = object(value, {"wellbeing", "fatigue", "pain", "areas", "otherArea", "newPain", "comment"});
    core::SportWellbeing result;
    result.wellbeing = optionalValue<int>(fields.value("wellbeing"), integer);
    result.fatigue = optionalValue<int>(fields.value("fatigue"), integer);
    result.pain = optionalValue<int>(fields.value("pain"), integer);
    result.areas = arrayValue<std::string>(fields.value("areas"), textValue);
    result.otherArea = textValue(fields.value("otherArea"));
    result.newPain = optionalValue<bool>(fields.value("newPain"), boolean);
    result.comment = textValue(fields.value("comment"));

    return result;
}
QJsonValue encodeSportTimerSegment(const core::SportTimerSegment& value)
{
    return QJsonObject{{"begin", QString::number(value.begin.time_since_epoch().count())},
                       {"end", QString::number(value.end.time_since_epoch().count())},
                       {"elapsed", QString::number((value.elapsed).count())},
                       {"active", value.active}};
}
core::SportTimerSegment decodeSportTimerSegment(const QJsonValue& value)
{
    const auto fields = object(value, {"begin", "end", "elapsed", "active"});
    core::SportTimerSegment result;
    result.begin = instantValue(fields.value("begin"));
    result.end = instantValue(fields.value("end"));
    result.elapsed = durationValue(fields.value("elapsed"));
    result.active = boolean(fields.value("active"));

    return result;
}
QJsonValue encodeSportAbsenceInterval(const core::SportAbsenceInterval& value)
{
    return QJsonObject{
        {"identifier", encodeText(value.identifier)},
        {"begin", QString::number(value.begin.time_since_epoch().count())},
        {"end", QString::number(value.end.time_since_epoch().count())},
        {"duration", (value.duration ? QJsonValue{QString::number((*value.duration).count())} : QJsonValue{})},
        {"active", (value.active ? QJsonValue{QString::number((*value.active).count())} : QJsonValue{})},
        {"decision", (value.decision ? QJsonValue{static_cast<int>(*value.decision)} : QJsonValue{})}};
}
core::SportAbsenceInterval decodeSportAbsenceInterval(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "begin", "end", "duration", "active", "decision"});
    core::SportAbsenceInterval result;
    result.identifier = textValue(fields.value("identifier"));
    result.begin = instantValue(fields.value("begin"));
    result.end = instantValue(fields.value("end"));
    result.duration = optionalValue<core::SportDuration>(fields.value("duration"), durationValue);
    result.active = optionalValue<core::SportDuration>(fields.value("active"), durationValue);
    result.decision = optionalValue<core::SportAbsenceDecision>(fields.value("decision"), decisionValue);

    return result;
}
QJsonValue encodeSportTimerState(const core::SportTimerState& value)
{
    return QJsonObject{
        {"begin", QString::number(value.begin.time_since_epoch().count())},
        {"observed", QString::number(value.observed.time_since_epoch().count())},
        {"paused", value.paused},
        {"clockWarning", value.clockWarning},
        {"segments", encodeArray(value.segments, [](const auto& item) { return encodeSportTimerSegment(item); })},
        {"absences", encodeArray(value.absences, [](const auto& item) { return encodeSportAbsenceInterval(item); })},
        {"manualDuration",
         (value.manualDuration ? QJsonValue{QString::number((*value.manualDuration).count())} : QJsonValue{})}};
}
core::SportTimerState decodeSportTimerState(const QJsonValue& value)
{
    const auto fields =
        object(value, {"begin", "observed", "paused", "clockWarning", "segments", "absences", "manualDuration"});
    core::SportTimerState result;
    result.begin = instantValue(fields.value("begin"));
    result.observed = instantValue(fields.value("observed"));
    result.paused = boolean(fields.value("paused"));
    result.clockWarning = boolean(fields.value("clockWarning"));
    result.segments = arrayValue<core::SportTimerSegment>(fields.value("segments"), decodeSportTimerSegment);
    result.absences = arrayValue<core::SportAbsenceInterval>(fields.value("absences"), decodeSportAbsenceInterval);
    result.manualDuration = optionalValue<core::SportDuration>(fields.value("manualDuration"), durationValue);

    return result;
}
QJsonValue encodeSportActualSet(const core::SportActualSet& value)
{
    return QJsonObject{{"identifier", encodeText(value.identifier)},
                       {"plannedIdentifier", encodeText(value.plannedIdentifier)},
                       {"side", static_cast<int>(value.side)},
                       {"state", static_cast<int>(value.state)},
                       {"repetitions", (value.repetitions ? QJsonValue{*value.repetitions} : QJsonValue{})},
                       {"load", encodeSportLoad(value.load)},
                       {"skipReason", encodeText(value.skipReason)}};
}
core::SportActualSet decodeSportActualSet(const QJsonValue& value)
{
    const auto fields =
        object(value, {"identifier", "plannedIdentifier", "side", "state", "repetitions", "load", "skipReason"});
    core::SportActualSet result;
    result.identifier = textValue(fields.value("identifier"));
    result.plannedIdentifier = textValue(fields.value("plannedIdentifier"));
    result.side = sideValue(fields.value("side"));
    result.state = stateValue(fields.value("state"));
    result.repetitions = optionalValue<int>(fields.value("repetitions"), integer);
    result.load = decodeSportLoad(fields.value("load"));
    result.skipReason = textValue(fields.value("skipReason"));

    return result;
}
QJsonValue encodeSportWorkoutItem(const core::SportWorkoutItem& value)
{
    return QJsonObject{
        {"plan", encodeSportProgramItem(value.plan)},
        {"actual", encodeArray(value.actual, [](const auto& item) { return encodeSportActualSet(item); })},
        {"replacesItem", encodeText(value.replacesItem)}};
}
core::SportWorkoutItem decodeSportWorkoutItem(const QJsonValue& value)
{
    const auto fields = object(value, {"plan", "actual", "replacesItem"});
    core::SportWorkoutItem result;
    result.plan = decodeSportProgramItem(fields.value("plan"));
    result.actual = arrayValue<core::SportActualSet>(fields.value("actual"), decodeSportActualSet);
    result.replacesItem = textValue(fields.value("replacesItem"));

    return result;
}
QJsonValue encodeSportWorkoutDraft(const core::SportWorkoutDraft& value)
{
    return QJsonObject{
        {"identifier", encodeText(value.identifier)},
        {"programIdentifier", encodeText(value.programIdentifier)},
        {"programRevision", encodeText(value.programRevision)},
        {"date", QDate{static_cast<int>(value.date.year()), static_cast<int>(static_cast<unsigned>(value.date.month())),
                       static_cast<int>(static_cast<unsigned>(value.date.day()))}
                     .toString(Qt::ISODate)},
        {"utcOffsetMinutes", value.utcOffsetMinutes},
        {"originalPlan", encodeSportSessionTemplate(value.originalPlan)},
        {"items", encodeArray(value.items, [](const auto& item) { return encodeSportWorkoutItem(item); })},
        {"timer", encodeSportTimerState(value.timer)},
        {"before", encodeSportWellbeing(value.before)},
        {"after", encodeSportWellbeing(value.after)},
        {"notes", encodeText(value.notes)}};
}
core::SportWorkoutDraft decodeSportWorkoutDraft(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "programIdentifier", "programRevision", "date", "utcOffsetMinutes",
                                       "originalPlan", "items", "timer", "before", "after", "notes"});
    core::SportWorkoutDraft result;
    result.identifier = textValue(fields.value("identifier"));
    result.programIdentifier = textValue(fields.value("programIdentifier"));
    result.programRevision = textValue(fields.value("programRevision"));
    result.date = dateValue(fields.value("date"));
    result.utcOffsetMinutes = integer(fields.value("utcOffsetMinutes"));
    result.originalPlan = decodeSportSessionTemplate(fields.value("originalPlan"));
    result.items = arrayValue<core::SportWorkoutItem>(fields.value("items"), decodeSportWorkoutItem);
    result.timer = decodeSportTimerState(fields.value("timer"));
    result.before = decodeSportWellbeing(fields.value("before"));
    result.after = decodeSportWellbeing(fields.value("after"));
    result.notes = textValue(fields.value("notes"));

    return result;
}
QJsonValue encodeSportWorkoutRevision(const core::SportWorkoutRevision& value)
{
    return QJsonObject{{"identifier", encodeText(value.identifier)},
                       {"parentRevision", encodeText(value.parentRevision)},
                       {"workout", encodeSportWorkoutDraft(value.workout)},
                       {"entirelySkipped", value.entirelySkipped}};
}
core::SportWorkoutRevision decodeSportWorkoutRevision(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "parentRevision", "workout", "entirelySkipped"});
    core::SportWorkoutRevision result;
    result.identifier = textValue(fields.value("identifier"));
    result.parentRevision = textValue(fields.value("parentRevision"));
    result.workout = decodeSportWorkoutDraft(fields.value("workout"));
    result.entirelySkipped = boolean(fields.value("entirelySkipped"));

    return result;
}
QJsonValue encodeSportCheckpoint(const core::SportCheckpoint& value)
{
    return QJsonObject{{"draft", encodeSportWorkoutDraft(value.draft)},
                       {"editorText", encodeMap(value.editorText)},
                       {"sequence", QString::number(value.sequence)}};
}
core::SportCheckpoint decodeSportCheckpoint(const QJsonValue& value)
{
    const auto fields = object(value, {"draft", "editorText", "sequence"});
    core::SportCheckpoint result;
    result.draft = decodeSportWorkoutDraft(fields.value("draft"));
    result.editorText = mapValue(fields.value("editorText"));
    result.sequence = unsignedText(fields.value("sequence"));

    return result;
}
[[nodiscard]] QByteArray document(const QString& kind, const QJsonValue& data)
{
    const auto bytes =
        QJsonDocument{QJsonObject{{"version", 1}, {"kind", kind}, {"data", data}}}.toJson(QJsonDocument::Compact);
    require(bytes.size() <= kMaximumDocumentBytes);

    return bytes;
}
[[nodiscard]] QJsonValue payload(const QByteArray& bytes, const QString& kind)
{
    require(bytes.size() <= kMaximumDocumentBytes);
    QJsonParseError error;
    const auto documentValue = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && documentValue.isObject());
    const auto fields = object(documentValue.object(), {"version", "kind", "data"});
    require(integer(fields.value("version")) == 1 && fields.value("kind").toString() == kind);

    return fields.value("data");
}
} // namespace
QByteArray SportDocumentCodec::encodeExercise(const core::SportExerciseRevision& value)
{
    const auto bytes = document("Exercise", encodeSportExerciseRevision(value));
    static_cast<void>(decodeExercise(bytes));

    return bytes;
}
core::SportExerciseRevision SportDocumentCodec::decodeExercise(const QByteArray& bytes)
{
    const auto result = decodeSportExerciseRevision(payload(bytes, "Exercise"));
    require(!core::validateSportExercise(result));

    return result;
}
QByteArray SportDocumentCodec::encodeProgram(const core::SportProgramRevision& value)
{
    const auto bytes = document("Program", encodeSportProgramRevision(value));
    static_cast<void>(decodeProgram(bytes));

    return bytes;
}
core::SportProgramRevision SportDocumentCodec::decodeProgram(const QByteArray& bytes)
{
    const auto result = decodeSportProgramRevision(payload(bytes, "Program"));
    require(!core::validateSportProgram(result));

    return result;
}
QByteArray SportDocumentCodec::encodeCheckpoint(const core::SportCheckpoint& value)
{
    const auto bytes = document("Checkpoint", encodeSportCheckpoint(value));
    static_cast<void>(decodeCheckpoint(bytes));

    return bytes;
}
core::SportCheckpoint SportDocumentCodec::decodeCheckpoint(const QByteArray& bytes)
{
    const auto result = decodeSportCheckpoint(payload(bytes, "Checkpoint"));
    require(!result.draft.identifier.empty() && result.draft.date.ok());
    require(!core::validateSportSession(result.draft.originalPlan));
    require(!result.draft.items.empty());
    require(!core::validateSportWellbeing(result.draft.before) && !core::validateSportWellbeing(result.draft.after));
    std::set<std::string> itemIdentifiers;
    for (const auto& item : result.draft.items)
    {
        require(itemIdentifiers.insert(item.plan.identifier).second && !item.actual.empty() &&
                item.actual.size() <= 100);
        require(!core::validateSportSession({"checkpoint", "Проверка", {item.plan}}));
        std::set<std::string> setIdentifiers;
        for (const auto& actual : item.actual)
        {
            require(!actual.identifier.empty() && setIdentifiers.insert(actual.identifier).second);
            require(static_cast<int>(actual.load.index()) == item.plan.exercise.loadKind);
            require(std::ranges::find(item.plan.exercise.sides, actual.side) != item.plan.exercise.sides.end());
            require(!core::validateSportLoad(actual.load, actual.state == core::SportSetState::Performed));
            if (actual.state == core::SportSetState::Performed)
            {
                require(actual.repetitions && *actual.repetitions >= 1 && *actual.repetitions <= 1000);
            }
            else
            {
                require(!actual.repetitions);
            }
        }
    }
    const auto& timer = result.draft.timer;
    if (timer.manualDuration)
    {
        require(*timer.manualDuration > core::SportDuration::zero() &&
                *timer.manualDuration <= std::chrono::hours{24 * 7});
    }
    std::set<std::string> intervalIdentifiers;
    for (const auto& interval : timer.absences)
    {
        require(!interval.identifier.empty() && intervalIdentifiers.insert(interval.identifier).second);
        require(interval.active.has_value() == interval.decision.has_value());
        if (interval.duration)
        {
            require(interval.end >= interval.begin && *interval.duration == interval.end - interval.begin);
        }
        if (interval.active && interval.duration)
        {
            require(*interval.active <= *interval.duration);
        }
        if (interval.decision == core::SportAbsenceDecision::Activity)
        {
            require(interval.duration && interval.active == interval.duration);
        }
        if (interval.decision == core::SportAbsenceDecision::Pause)
        {
            require(interval.active == core::SportDuration::zero());
        }
    }

    return result;
}
QByteArray SportDocumentCodec::encodeRevision(const core::SportWorkoutRevision& value)
{
    const auto bytes = document("Revision", encodeSportWorkoutRevision(value));
    static_cast<void>(decodeRevision(bytes));

    return bytes;
}
core::SportWorkoutRevision SportDocumentCodec::decodeRevision(const QByteArray& bytes)
{
    const auto result = decodeSportWorkoutRevision(payload(bytes, "Revision"));
    const auto checked = core::SportWorkoutService{}.finish(result.workout, result.identifier);
    require(std::holds_alternative<core::SportWorkoutRevision>(checked));
    require(std::get<core::SportWorkoutRevision>(checked).entirelySkipped == result.entirelySkipped);
    require(result.parentRevision != result.identifier);

    return result;
}
QByteArray SportDocumentCodec::encodeProgramCheckpoint(const core::SportProgramCheckpoint& value)
{
    const auto bytes = document("ProgramCheckpoint", QJsonObject{{"program", encodeSportProgramRevision(value.program)},
                                                                 {"editorText", encodeMap(value.editorText)},
                                                                 {"sequence", QString::number(value.sequence)}});
    static_cast<void>(decodeProgramCheckpoint(bytes));

    return bytes;
}
core::SportProgramCheckpoint SportDocumentCodec::decodeProgramCheckpoint(const QByteArray& bytes)
{
    const auto fields = object(payload(bytes, "ProgramCheckpoint"), {"program", "editorText", "sequence"});
    core::SportProgramCheckpoint result;
    result.program = decodeSportProgramRevision(fields.value("program"));
    result.editorText = mapValue(fields.value("editorText"));
    result.sequence = unsignedText(fields.value("sequence"));
    require(!result.program.identifier.empty() && !result.program.revision.empty());

    return result;
}
} // namespace msa::infra
