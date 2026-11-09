#include "FakeClock.h"
#include "InMemoryTaskRepository.h"

#include "msa/core/TaskService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

using std::chrono::year;

TEST(TaskServiceTest, CreatesWithClockAndAllowsSingleCharacterAndPastDeadline)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5} + std::chrono::seconds{123}};
    InMemoryTaskRepository repository;
    TaskService service{clock, repository};
    EXPECT_TRUE(service.tasks().empty());
    const auto identifier = std::get<std::int64_t>(service.addTask("Я", year{2020} / 1 / 1));
    const auto task = repository.find(identifier);
    ASSERT_TRUE(task);
    EXPECT_GT(identifier, 0);
    EXPECT_EQ(task->title, "Я");
    EXPECT_EQ(task->createdAt, clock.now());
    EXPECT_EQ(task->dueDate, year{2020} / 1 / 1);
    EXPECT_EQ(task->priority, TaskPriority::Normal);
    EXPECT_FALSE(task->done);
    clock.advance(std::chrono::seconds{1});
    const auto second = std::get<std::int64_t>(service.addTask("A", std::nullopt));
    EXPECT_EQ(repository.find(second)->createdAt, clock.now());
    EXPECT_FALSE(repository.find(second)->dueDate);
}

TEST(TaskServiceTest, RejectsInvalidInputWithoutWriting)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryTaskRepository repository;
    TaskService service{clock, repository};
    const std::vector<Task> invalid{
        Task{.title = ""},
        Task{.title = "Я", .dueDate = year{2026} / 2 / 30},
        Task{.title = "Я", .priority = static_cast<TaskPriority>(255)}};
    const auto identifier = std::get<std::int64_t>(service.addTask("Исходная", std::nullopt));
    for (const auto& task : invalid) {
        const auto result = service.addTask(task.title, task.dueDate, task.priority);
        ASSERT_TRUE(std::holds_alternative<TaskRejection>(result));
        EXPECT_FALSE(std::get<TaskRejection>(result).reason.empty());
        EXPECT_TRUE(std::holds_alternative<TaskRejection>(service.updateTask(
            identifier, task.title, task.dueDate, task.priority)));
        EXPECT_EQ(repository.find(identifier)->title, "Исходная");
        EXPECT_FALSE(repository.find(identifier)->dueDate);
        EXPECT_EQ(repository.find(identifier)->priority, TaskPriority::Normal);
    }
    EXPECT_EQ(repository.all().size(), 1U);
}

TEST(TaskServiceTest, UpdatesAndReopensWithoutChangingOtherFields)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryTaskRepository repository;
    TaskService service{clock, repository};
    const auto createdAt = clock.now();
    const auto identifier = std::get<std::int64_t>(service.addTask("Первая", std::nullopt));
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.setTaskDone(identifier, true)));
    clock.advance(std::chrono::days{1});
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.updateTask(
        identifier, "Новая", year{2026} / 12 / 1, TaskPriority::High)));
    auto task = repository.find(identifier);
    ASSERT_TRUE(task);
    EXPECT_TRUE(task->done);
    EXPECT_EQ(task->createdAt, createdAt);
    EXPECT_EQ(task->title, "Новая");
    EXPECT_EQ(task->dueDate, year{2026} / 12 / 1);
    EXPECT_EQ(task->priority, TaskPriority::High);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.setTaskDone(identifier, false)));
    EXPECT_FALSE(repository.find(identifier)->done);
    EXPECT_EQ(repository.find(identifier)->title, "Новая");
    EXPECT_EQ(repository.find(identifier)->createdAt, createdAt);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(service.updateTask(
        identifier, "Новая", std::nullopt, TaskPriority::Low)));
    EXPECT_FALSE(repository.find(identifier)->dueDate);
    EXPECT_TRUE(std::holds_alternative<TaskRejection>(service.setTaskDone(identifier + 1, true)));
    EXPECT_TRUE(std::holds_alternative<TaskRejection>(service.updateTask(
        identifier + 1, "Нет", std::nullopt, TaskPriority::Normal)));
    EXPECT_EQ(repository.all().size(), 1U);
}

TEST(TaskServiceTest, SortsAllTasksAndReturnsIndependentCopies)
{
    FakeClock clock{std::chrono::local_days{year{2026} / 11 / 5}};
    InMemoryTaskRepository repository;
    TaskService service{clock, repository};
    const auto finished = repository.add(Task{.title = "Готово", .priority = TaskPriority::High, .done = true});
    const auto low = repository.add(Task{.title = "Низкий", .dueDate = year{2020} / 1 / 1, .priority = TaskPriority::Low});
    const auto undated = repository.add(Task{.title = "Без срока", .priority = TaskPriority::High});
    const auto later = repository.add(Task{.title = "Позже", .dueDate = year{2027} / 1 / 1, .priority = TaskPriority::High});
    const auto early = repository.add(Task{.title = "Раньше", .dueDate = year{2026} / 1 / 1, .priority = TaskPriority::High});
    const auto tied = repository.add(Task{.title = "Равная", .dueDate = year{2026} / 1 / 1, .priority = TaskPriority::High});
    const auto finishedUndated = repository.add(Task{.title = "Готово без срока", .priority = TaskPriority::High, .done = true});
    const auto finishedDated = repository.add(Task{.title = "Готово со сроком", .dueDate = year{2026} / 1 / 1, .priority = TaskPriority::High, .done = true});
    auto tasks = service.tasks();
    std::vector<std::int64_t> identifiers;
    for (const auto& task : tasks) {
        identifiers.push_back(task.identifier);
    }
    EXPECT_EQ(identifiers, (std::vector<std::int64_t>{early, tied, later, undated, low,
        finishedDated, finished, finishedUndated}));
    tasks.front().title = "Копия";
    EXPECT_EQ(repository.find(early)->title, "Раньше");
}

}  // namespace
}  // namespace msa::core

