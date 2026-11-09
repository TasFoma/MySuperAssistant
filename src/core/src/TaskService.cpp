#include "msa/core/TaskService.h"

#include "msa/core/detail/Utf8Text.h"

#include <algorithm>

namespace msa::core {
namespace {

[[nodiscard]] std::optional<TaskRejection> validate(const std::string& title,
    std::optional<std::chrono::year_month_day> dueDate, TaskPriority priority)
{
    if (text::charCount(title) < 1) {
        return TaskRejection{"Введите название задачи."};
    }
    if (dueDate && !dueDate->ok()) {
        return TaskRejection{"Укажите корректную дату срока."};
    }
    if (priority != TaskPriority::Low && priority != TaskPriority::Normal
        && priority != TaskPriority::High) {
        return TaskRejection{"Выберите допустимый приоритет задачи."};
    }

    return std::nullopt;
}

}  // namespace

TaskService::TaskService(const Clock& clock, TaskRepository& repository)
    : m_clock{clock}, m_repository{repository}
{
}

TaskWriteResult TaskService::addTask(const std::string& title,
    std::optional<std::chrono::year_month_day> dueDate, TaskPriority priority)
{
    if (const auto rejection = validate(title, dueDate, priority)) {
        return *rejection;
    }

    return m_repository.add(Task{.title = title, .dueDate = dueDate, .priority = priority,
                                 .createdAt = m_clock.now()});
}

TaskChangeResult TaskService::updateTask(std::int64_t identifier, const std::string& title,
    std::optional<std::chrono::year_month_day> dueDate, TaskPriority priority)
{
    if (const auto rejection = validate(title, dueDate, priority)) {
        return *rejection;
    }
    auto task = m_repository.find(identifier);
    if (!task) {
        return TaskRejection{"Задача не найдена."};
    }
    task->title = title;
    task->dueDate = dueDate;
    task->priority = priority;
    if (!m_repository.update(*task)) {
        return TaskRejection{"Задача не найдена."};
    }

    return std::monostate{};
}

TaskChangeResult TaskService::setTaskDone(std::int64_t identifier, bool done)
{
    auto task = m_repository.find(identifier);
    if (!task) {
        return TaskRejection{"Задача не найдена."};
    }
    task->done = done;
    if (!m_repository.update(*task)) {
        return TaskRejection{"Задача не найдена."};
    }

    return std::monostate{};
}

std::vector<Task> TaskService::tasks() const
{
    auto result = m_repository.all();
    std::ranges::sort(result, [](const Task& left, const Task& right) {
        if (left.done != right.done) {
            return !left.done;
        }
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        if (left.dueDate.has_value() != right.dueDate.has_value()) {
            return left.dueDate.has_value();
        }
        if (left.dueDate != right.dueDate) {
            return left.dueDate < right.dueDate;
        }

        return left.identifier < right.identifier;
    });

    return result;
}

}  // namespace msa::core

