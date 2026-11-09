#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>

namespace msa::infra
{
/// @brief Внутренние JSON-кадры C++-процесса распознавания, не сетевой протокол сервера.
class VoiceProtocol final
{
public:
    static constexpr qsizetype kMaximumFrameBytes{65536}; ///< @brief Предел без LF.
    /// @brief Добавляет version=1 и LF; неверный/слишком большой объект отклоняется.
    [[nodiscard]] static QByteArray encode(QJsonObject object);
    /// @brief Накапливает байты до LF, проверяет JSON/version и отдаёт полные объекты.
    [[nodiscard]] static QList<QJsonObject> consume(QByteArray& pending, const QByteArray& bytes);
    /// @brief Положительный ID записи в виде десятичной строки без потери точности.
    [[nodiscard]] static quint64 session(const QJsonObject& object);
};
} // namespace msa::infra
