#pragma once

#include "msa/core/EventRepository.h"
#include "msa/infra/SqliteDatabase.h"

namespace msa::infra {

/// @brief SQLite-хранилище серий событий без расчёта отдельных вхождений.
/// База со схемой sqliteMigrations() живёт дольше репозитория, в том же потоке.
class SqliteEventRepository final : public core::EventRepository {
public:
    /// @brief Запоминает невладеющую ссылку на базу с предметной схемой.
    explicit SqliteEventRepository(SqliteDatabase& database);
    /// @brief Сохраняет копию события с нулевым ID и назначает положительный ID.
    [[nodiscard]] std::int64_t add(const core::Event& item) override;
    /// @brief Возвращает копию события либо nullopt.
    [[nodiscard]] std::optional<core::Event> find(std::int64_t identifier) const override;
    /// @brief Возвращает независимые копии всех серий без гарантии порядка.
    [[nodiscard]] std::vector<core::Event> all() const override;
    /// @brief Заменяет всю серию по ID; false при отсутствии без вставки.
    [[nodiscard]] bool update(const core::Event& item) override;
    /// @brief Удаляет серию по ID; false при отсутствии. Отметки предупреждений не меняет.
    [[nodiscard]] bool remove(std::int64_t identifier) override;

private:
    SqliteDatabase& m_database;
};

}  // namespace msa::infra

