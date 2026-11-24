#include "msa/infra/SportProgramCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResource>
#include <QStringList>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <utility>

// Заготовки входят в тот же статический ресурс infra, что и карточки.
static void initializeSportProgramResources()
{
    Q_INIT_RESOURCE(sport_content);
}
namespace msa::infra
{
namespace
{
constexpr std::size_t kExpectedProgramCount = 2;
constexpr double kSupportedProgramPackageVersion = 1.0;
constexpr qsizetype kMaximumProgramPackageBytes = 1024 * 1024;
constexpr qsizetype kMaximumProgramTextCharacters = 16384;
constexpr qsizetype kMaximumPackageItems = 100;
void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error{"Некорректный или неподдерживаемый пакет заготовок спорта."};
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
[[nodiscard]] std::string text(const QJsonValue& value)
{
    require(value.isString());
    const auto result = value.toString();
    require(!result.trimmed().isEmpty() && result.size() <= kMaximumProgramTextCharacters);

    return result.toStdString();
}
[[nodiscard]] QJsonArray array(const QJsonValue& value)
{
    require(value.isArray() && value.toArray().size() <= kMaximumPackageItems);

    return value.toArray();
}
[[nodiscard]] std::vector<std::string> strings(const QJsonValue& value)
{
    std::vector<std::string> result;
    for (const auto& item : array(value))
    {
        result.push_back(text(item));
    }

    return result;
}
[[nodiscard]] core::SportProgramBlueprint decodeProgram(const QJsonValue& value)
{
    const auto fields = object(value, {"identifier", "revision", "code", "title", "description", "authorship",
                                       "equipment", "conditions", "dosage", "sessions"});
    require(text(fields.value("dosage")) == "user-defined");
    core::SportProgramBlueprint result;
    result.identifier = text(fields.value("identifier"));
    result.revision = text(fields.value("revision"));
    result.code = text(fields.value("code"));
    result.title = text(fields.value("title"));
    result.description = text(fields.value("description"));
    result.authorship = text(fields.value("authorship"));
    result.equipment = strings(fields.value("equipment"));
    result.conditions = strings(fields.value("conditions"));
    for (const auto& valueSession : array(fields.value("sessions")))
    {
        const auto sessionFields = object(valueSession, {"identifier", "title", "description", "items"});
        core::SportProgramBlueprintSession session;
        session.identifier = text(sessionFields.value("identifier"));
        session.title = text(sessionFields.value("title"));
        session.description = text(sessionFields.value("description"));
        for (const auto& valueItem : array(sessionFields.value("items")))
        {
            const auto itemFields =
                object(valueItem, {"identifier", "exerciseIdentifier", "instructions", "alternatives"});
            core::SportProgramBlueprintItem item;
            item.identifier = text(itemFields.value("identifier"));
            item.exerciseIdentifier = text(itemFields.value("exerciseIdentifier"));
            item.instructions = text(itemFields.value("instructions"));
            for (const auto& valueAlternative : array(itemFields.value("alternatives")))
            {
                const auto alternativeFields = object(valueAlternative, {"exerciseIdentifier", "description"});
                item.alternatives.push_back({text(alternativeFields.value("exerciseIdentifier")),
                                             text(alternativeFields.value("description"))});
            }
            session.items.push_back(std::move(item));
        }
        result.sessions.push_back(std::move(session));
    }

    return result;
}
} // namespace
std::vector<core::SportProgramBlueprint>
loadBundledSportProgramCatalog(std::span<const core::SportExerciseRevision> catalog)
{
    initializeSportProgramResources();
    QFile file{":/msa/sport/SportPrograms.json"};
    if (!file.open(QIODevice::ReadOnly))
    {
        throw std::runtime_error{"Офлайн-заготовки программ не найдены."};
    }
    require(file.size() >= 0 && file.size() <= kMaximumProgramPackageBytes);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    require(error.error == QJsonParseError::NoError && document.isObject());
    const auto package = object(document.object(), {"version", "programs"});
    require(package.value("version").isDouble() &&
            package.value("version").toDouble() == kSupportedProgramPackageVersion);
    std::set<std::string> identifiers;
    std::set<std::string> codes;
    std::vector<core::SportProgramBlueprint> result;
    for (const auto& value : array(package.value("programs")))
    {
        auto program = decodeProgram(value);
        require(identifiers.insert(program.identifier).second && codes.insert(program.code).second);
        if (const auto problem = core::validateSportProgramBlueprint(program, catalog))
        {
            throw std::runtime_error{problem->reason};
        }
        result.push_back(std::move(program));
    }
    require(result.size() == kExpectedProgramCount && codes.contains("P1") && codes.contains("P3"));

    return result;
}
} // namespace msa::infra
