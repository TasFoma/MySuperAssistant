#include "msa/infra/SportCatalog.h"

#include "msa/infra/SportDocumentCodec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResource>
#include <set>
#include <stdexcept>

// Ресурс статической infra регистрируется из глобального пространства имён.
static void initializeSportResources()
{
    Q_INIT_RESOURCE(sport_content);
}
namespace msa::infra
{
std::vector<core::SportExerciseRevision> loadBundledSportCatalog()
{
    initializeSportResources();
    QFile file{":/msa/sport/SportExercises.json"};
    if (!file.open(QIODevice::ReadOnly))
    {
        throw std::runtime_error{"Офлайн-библиотека спорта не найдена."};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        document.object().value("version").toInt() != 1 || !document.object().value("exercises").isArray())
    {
        throw std::runtime_error{"Офлайн-библиотека повреждена."};
    }
    std::vector<core::SportExerciseRevision> result;
    std::set<std::string> identifiers;
    for (const auto& item : document.object().value("exercises").toArray())
    {
        const auto exercise =
            SportDocumentCodec::decodeExercise(QJsonDocument{item.toObject()}.toJson(QJsonDocument::Compact));
        if (!identifiers.insert(exercise.identifier).second)
        {
            throw std::runtime_error{"В библиотеке повторяется идентичность."};
        }
        result.push_back(exercise);
    }
    if (document.object().value("plannedCount").toInt() != static_cast<int>(result.size()))
    {
        throw std::runtime_error{"Неполный пакет библиотеки спорта."};
    }

    return result;
}
} // namespace msa::infra
