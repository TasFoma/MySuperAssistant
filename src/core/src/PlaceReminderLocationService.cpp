#include "msa/core/PlaceReminderLocationService.h"

#include "msa/core/NavigationService.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace msa::core
{
namespace
{

[[nodiscard]] bool matches(const std::optional<Reminder>& current, const PlaceReminder& snapshot)
{
    if (!current)
    {
        return false;
    }
    const auto* place = std::get_if<PlaceReminder>(&*current);

    return place && place->identifier == snapshot.identifier && place->title == snapshot.title &&
           place->place == snapshot.place && place->status == ReminderStatus::Pending && !place->coordinates;
}

[[nodiscard]] ExternalServiceError changedReminder()
{
    return {ExternalServiceErrorKind::InvalidRequest,
            "Напоминание изменилось или уже имеет точку. Выберите место заново."};
}

} // namespace

PlaceReminderLocationService::PlaceReminderLocationService(ReminderRepository& repository, Geocoder& geocoder) :
    m_repository{repository}, m_geocoder{geocoder}
{
}

ExternalServiceResult<PlaceReminderLocationProposal>
PlaceReminderLocationService::prepare(std::int64_t reminderIdentifier, GeoCoordinates reference)
{
    if (m_preparing)
    {
        return ExternalServiceError{ExternalServiceErrorKind::Unavailable, "Поиск места уже выполняется."};
    }
    if (reminderIdentifier <= 0 || !validCoordinates(reference))
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Укажите напоминание и опорное местоположение."};
    }
    std::erase_if(m_pending, [reminderIdentifier](const auto& item)
                  { return item.second.snapshot.identifier == reminderIdentifier; });
    const auto reminder = m_repository.find(reminderIdentifier);
    if (!reminder)
    {
        return changedReminder();
    }
    const auto* place = std::get_if<PlaceReminder>(&*reminder);
    if (!place || place->status != ReminderStatus::Pending || place->coordinates)
    {
        return changedReminder();
    }
    const auto snapshot = *place;
    ExternalServiceResult<std::vector<GeocodedPlace>> result;
    m_preparing = true;
    try
    {
        result = m_geocoder.search(snapshot.place);
    }
    catch (...)
    {
        m_preparing = false;
        throw;
    }
    m_preparing = false;
    if (const auto* error = std::get_if<ExternalServiceError>(&result))
    {
        return *error;
    }
    if (!matches(m_repository.find(reminderIdentifier), snapshot))
    {
        return changedReminder();
    }
    auto candidates = std::get<std::vector<GeocodedPlace>>(std::move(result));
    std::ranges::stable_sort(
        candidates, [reference](const GeocodedPlace& left, const GeocodedPlace& right)
        { return geographicDistance(reference, left.coordinates) < geographicDistance(reference, right.coordinates); });
    if (m_nextIdentifier == std::numeric_limits<std::int64_t>::max())
    {
        return ExternalServiceError{ExternalServiceErrorKind::Unavailable,
                                    "Исчерпаны номера предложений. Перезапустите сервис."};
    }
    PlaceReminderLocationProposal proposal{m_nextIdentifier++, reminderIdentifier, snapshot.place,
                                           std::move(candidates)};
    m_pending.emplace(proposal.identifier, PendingProposal{snapshot, proposal});

    return proposal;
}

ExternalServiceResult<GeoCoordinates> PlaceReminderLocationService::confirm(std::int64_t proposalIdentifier,
                                                                            std::size_t candidateIndex)
{
    const auto pending = m_pending.find(proposalIdentifier);
    if (pending == m_pending.end() || candidateIndex >= pending->second.proposal.candidates.size())
    {
        return ExternalServiceError{ExternalServiceErrorKind::InvalidRequest,
                                    "Выберите место из действующего предложения."};
    }
    const auto current = m_repository.find(pending->second.snapshot.identifier);
    if (!matches(current, pending->second.snapshot))
    {
        m_pending.erase(pending);
        return changedReminder();
    }
    auto updated = std::get<PlaceReminder>(*current);
    const auto coordinates = pending->second.proposal.candidates[candidateIndex].coordinates;
    updated.coordinates = coordinates;
    if (!m_repository.update(updated))
    {
        m_pending.erase(pending);
        return changedReminder();
    }
    m_pending.erase(pending);

    return coordinates;
}

bool PlaceReminderLocationService::cancel(std::int64_t proposalIdentifier)
{
    return m_pending.erase(proposalIdentifier) != 0;
}

} // namespace msa::core
