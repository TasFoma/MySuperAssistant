#pragma once

#include "msa/core/ClientPlannerAction.h"
#include "msa/infra/ClientProtocol.h"

#include <QJsonObject>

namespace msa::infra
{
/// @brief Внутренняя часть ClientProtocol: методы форм планировщика версии 1.
class ClientPlannerProtocol final
{
public:
    /// @brief Преобразует типизированное действие в параметры существующего метода.
    [[nodiscard]] static ClientWireRequest request(const core::ClientPlannerAction& action);
    /// @brief Проверяет точную форму подтверждения и ID изменяемой записи.
    static void validateResult(const core::ClientPlannerAction& action, const QJsonObject& result);
};
} // namespace msa::infra
