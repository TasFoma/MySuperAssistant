#pragma once

#include "msa/core/Geocoder.h"
#include "msa/core/ReminderRepository.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace msa::core
{

/// @brief Варианты для явного подтверждения; изменение копии не меняет предложение сервиса.
struct PlaceReminderLocationProposal
{
    /// @brief Идентификатор предложения в пределах живого экземпляра сервиса.
    std::int64_t identifier{};
    /// @brief Напоминание, для которого найдены варианты.
    std::int64_t reminderIdentifier{};
    /// @brief Название места, сохранённое в напоминании.
    std::string place{};
    /// @brief Кандидаты от ближайшего к опорной точке; ни один не выбран автоматически.
    std::vector<GeocodedPlace> candidates{};
};

/// @brief Поиск и подтверждение фиксированной точки геонапоминания без фонового обхода.
/// Использование последовательное, в одном потоке; сетевой вызов вне транзакции.
class PlaceReminderLocationService
{
public:
    /// @brief Зависимости должны пережить сервис.
    PlaceReminderLocationService(ReminderRepository& repository, Geocoder& geocoder);
    /// @brief Готовит варианты только для Pending без координат; перечитывает после сети.
    /// Новая попытка для того же напоминания отменяет прежнее предложение.
    [[nodiscard]] ExternalServiceResult<PlaceReminderLocationProposal> prepare(std::int64_t reminderIdentifier,
                                                                               GeoCoordinates reference);
    /// @brief Закрепляет выбранного кандидата, если напоминание не изменилось.
    /// Повторно перечитывает запись; ошибки хранилища — исключения, предложение остаётся для повтора.
    [[nodiscard]] ExternalServiceResult<GeoCoordinates> confirm(std::int64_t proposalIdentifier,
                                                                std::size_t candidateIndex);
    /// @brief Отменяет предложение без изменения напоминания; false при неизвестном ID.
    [[nodiscard]] bool cancel(std::int64_t proposalIdentifier);

private:
    struct PendingProposal
    {
        PlaceReminder snapshot{};
        PlaceReminderLocationProposal proposal{};
    };

    ReminderRepository& m_repository;
    Geocoder& m_geocoder;
    std::map<std::int64_t, PendingProposal> m_pending{};
    std::int64_t m_nextIdentifier{1};
    bool m_preparing{false};
};

} // namespace msa::core
