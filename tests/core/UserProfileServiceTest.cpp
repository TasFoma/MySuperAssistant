#include "InMemoryUserProfileRepository.h"

#include "msa/core/UserProfileService.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace msa::core {
namespace {

TEST(UserProfileServiceTest, TrimsEdgesAndPreservesMultilineFactsAndDuplicates)
{
    InMemoryUserProfileRepository repository;
    UserProfileService service{repository};
    EXPECT_TRUE(service.profile().facts.empty());
    const std::vector<std::string> inputs{
        " \tЯ\r\n", "\xC2\xA0Люблю  чай\xE3\x80\x80", "  Первая строка\n  Вторая строка  ", "Я"};
    for (const auto& input : inputs) {
        EXPECT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact(input)));
    }
    EXPECT_EQ(service.profile().facts,
        (std::vector<std::string>{"Я", "Люблю  чай", "Первая строка\n  Вторая строка", "Я"}));
    auto copy = service.profile();
    copy.facts.front() = "Изменённая копия";
    EXPECT_EQ(repository.load().facts.front(), "Я");
}

TEST(UserProfileServiceTest, KeepsAllFactsBeyondOldLimitAndAcrossServiceInstances)
{
    InMemoryUserProfileRepository repository;
    repository.save(UserProfile{{"Важный давний факт"}});
    UserProfileService service{repository};
    auto expected = repository.load().facts;
    constexpr int kAddedFactCount = 100;
    for (int index = 0; index < kAddedFactCount; ++index) {
        const auto fact = "Факт " + std::to_string(index);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(service.rememberFact(fact)));
        expected.push_back(fact);
    }
    UserProfileService nextService{repository};
    EXPECT_EQ(nextService.profile().facts, expected);
    ASSERT_TRUE(std::holds_alternative<std::monostate>(nextService.rememberFact("Ещё один")));
    expected.push_back("Ещё один");
    EXPECT_EQ(service.profile().facts, expected);
}

/// @brief Проверяет отсутствие обращений при отказе и проброс технических ошибок.
class FailingUserProfileRepository final : public UserProfileRepository {
public:
    explicit FailingUserProfileRepository(bool failOnLoad = true);
    [[nodiscard]] UserProfile load() const override;
    void save(const UserProfile& profile) override;
    [[nodiscard]] int saveCalls() const;

private:
    bool m_failOnLoad{true};
    int m_saveCalls{0};
};

FailingUserProfileRepository::FailingUserProfileRepository(bool failOnLoad)
    : m_failOnLoad{failOnLoad}
{
}

UserProfile FailingUserProfileRepository::load() const
{
    if (m_failOnLoad) {
        throw std::runtime_error{"load failed"};
    }

    return UserProfile{{"Сохранённый факт"}};
}

void FailingUserProfileRepository::save([[maybe_unused]] const UserProfile& profile)
{
    ++m_saveCalls;
    throw std::runtime_error{"save failed"};
}

int FailingUserProfileRepository::saveCalls() const
{
    return m_saveCalls;
}

TEST(UserProfileServiceTest, RejectsEmptyAndUnicodeWhitespaceWithoutAccessingRepository)
{
    FailingUserProfileRepository repository;
    UserProfileService service{repository};
    const std::vector<std::string> inputs{
        "", " \t\r\n\v\f", "\xC2\xA0\xE2\x80\x83\xE2\x80\xA8\xE2\x80\xAF\xE3\x80\x80"};
    for (const auto& input : inputs) {
        const auto result = service.rememberFact(input);
        ASSERT_TRUE(std::holds_alternative<UserProfileRejection>(result));
        EXPECT_FALSE(std::get<UserProfileRejection>(result).reason.empty());
    }
    EXPECT_EQ(repository.saveCalls(), 0);
}

TEST(UserProfileServiceTest, PropagatesStorageErrors)
{
    FailingUserProfileRepository repository;
    UserProfileService service{repository};
    EXPECT_THROW(static_cast<void>(service.profile()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(service.rememberFact("Новый факт")), std::runtime_error);
    EXPECT_EQ(repository.saveCalls(), 0);
    FailingUserProfileRepository saveRepository{false};
    UserProfileService saveService{saveRepository};
    EXPECT_THROW(static_cast<void>(saveService.rememberFact("Новый факт")), std::runtime_error);
    EXPECT_EQ(saveRepository.saveCalls(), 1);
}

}  // namespace
}  // namespace msa::core

