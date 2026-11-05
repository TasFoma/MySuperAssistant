#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Общее хранение копий записей для тестовых репозиториев.
template <typename ItemType>
class InMemoryStore {
public:
    /// @brief Назначает новый ID; ожидает несохранённую запись с нулевым ID.
    [[nodiscard]] std::int64_t add(const ItemType& item)
    {
        if (identifierOf(item) != 0) {
            throw std::invalid_argument{"Новая запись должна иметь нулевой ID"};
        }
        if (m_nextIdentifier == std::numeric_limits<std::int64_t>::max()) {
            throw std::overflow_error{"Исчерпаны идентификаторы"};
        }
        auto copy = item;
        setIdentifier(copy, m_nextIdentifier);
        m_items.emplace(m_nextIdentifier, std::move(copy));

        return m_nextIdentifier++;
    }

    /// @brief Возвращает независимую копию записи.
    [[nodiscard]] std::optional<ItemType> find(std::int64_t identifier) const
    {
        const auto found = m_items.find(identifier);
        if (found == m_items.end()) {
            return std::nullopt;
        }

        return found->second;
    }

    /// @brief Возвращает независимые копии записей.
    [[nodiscard]] std::vector<ItemType> all() const
    {
        std::vector<ItemType> result;
        result.reserve(m_items.size());
        for (const auto& [identifier, item] : m_items) {
            result.push_back(item);
        }

        return result;
    }

    /// @brief Заменяет существующую запись, не создавая новую.
    [[nodiscard]] bool update(const ItemType& item)
    {
        const auto found = m_items.find(identifierOf(item));
        if (found == m_items.end()) {
            return false;
        }
        found->second = item;

        return true;
    }

    /// @brief Удаляет существующую запись.
    [[nodiscard]] bool remove(std::int64_t identifier)
    {
        return m_items.erase(identifier) != 0;
    }

private:
    [[nodiscard]] static std::int64_t identifierOf(const ItemType& item)
    {
        if constexpr (requires { item.identifier; }) {
            return item.identifier;
        } else {
            return std::visit([](const auto& value) { return value.identifier; }, item);
        }
    }

    static void setIdentifier(ItemType& item, std::int64_t identifier)
    {
        if constexpr (requires { item.identifier; }) {
            item.identifier = identifier;
        } else {
            std::visit([identifier](auto& value) { value.identifier = identifier; }, item);
        }
    }

    std::map<std::int64_t, ItemType> m_items{};
    std::int64_t m_nextIdentifier{1};
};

}  // namespace msa::core

