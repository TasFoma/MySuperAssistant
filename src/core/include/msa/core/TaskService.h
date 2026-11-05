#pragma once

#include "msa/core/Clock.h"
#include "msa/core/TaskRepository.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {

/// @brief Причина отказа операции с задачей.
struct TaskRejection {
    std::string reason;
};

/// @brief ID созданной задачи либо причина отказа.
using TaskWriteResult = std::variant<std::int64_t, TaskRejection>;
/// @brief Успех изменения либо причина отказа.
using TaskChangeResult = std::variant<std::monostate, TaskRejection>;

/// @brief Предметные операции с задачами; технические исключения пробрасываются.
class TaskService {
public:
    /// @brief Зависимости не принадлежат сервису и должны жить дольше него.
    TaskService(const Clock& clock, TaskRepository& repository);
    /// @brief Создаёт незавершённую задачу с текущим временем; прошлый срок допустим.
    /// Заголовок содержит минимум одну кодовую точку, дата и приоритет корректны.
    [[nodiscard]] TaskWriteResult addTask(const std::string& title,
        std::optional<std::chrono::year_month_day> dueDate,
        TaskPriority priority = TaskPriority::Normal);
    /// @brief Меняет поля, сохраняя время создания и завершённость; отсутствие — отказ.
    [[nodiscard]] TaskChangeResult updateTask(std::int64_t identifier, const std::string& title,
        std::optional<std::chrono::year_month_day> dueDate, TaskPriority priority);
    /// @brief Устанавливает или снимает завершённость; отсутствие — отказ.
    [[nodiscard]] TaskChangeResult setTaskDone(std::int64_t identifier, bool done);
    /// @brief Все задачи: открытые, приоритет по убыванию, срок по возрастанию,
    /// без срока после датированных внутри приоритета, затем ID по возрастанию.
    [[nodiscard]] std::vector<Task> tasks() const;

private:
    const Clock& m_clock;
    TaskRepository& m_repository;
};

}  // namespace msa::core

