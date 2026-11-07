#pragma once

#include <msa/infra/ProtocolMessage.h>

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonValue>

#include <variant>
#include <vector>

namespace msa::infra
{
/// Чистый JSON-кодек конвертов и параметров; не выполняет команды и не хранит авторизацию.
class ProtocolCodec
{
public:
    /// Разобрать UTF-8 кадр без завершающего LF; неизвестные поля запрещены.
    [[nodiscard]] static ProtocolDecodeResult decode(QByteArrayView frame);
    /// Проверить сообщение и добавить LF; вместо обрезки вернуть response_too_large.
    [[nodiscard]] static std::variant<QByteArray, ProtocolDecodeError> encode(const ProtocolMessage& message);
    /// Проверить точное строковое представление положительного int64 ID.
    [[nodiscard]] static bool isEntityId(const QJsonValue& value);
};

/// Результат приёма блока: завершённые кадры в порядке поступления и возможный фатальный отказ.
struct ProtocolFrameBatch
{
    std::vector<QByteArray> frames;
    bool invalidFrame{};
};

/// Ограниченный накопитель байтов одного соединения. После отказа принимает только reset.
class ProtocolFrameDecoder
{
public:
    /// Выделить строки, сохранив неполный UTF-8 до следующего блока; JSON разбирает ProtocolCodec.
    [[nodiscard]] ProtocolFrameBatch append(QByteArrayView bytes);
    /// Отбросить хвост и ошибку при закрытии/новом соединении; EOF не создаёт кадр.
    void reset();

private:
    QByteArray m_pending;
    bool m_failed{};
};
} // namespace msa::infra
