add_executable(msa_core_tests
    src/MusicPlaylistTest.cpp
    src/FakeWebSearchProvider.cpp
    src/WebSearchProviderTest.cpp
    src/InMemoryGeocodingCacheRepository.cpp
    src/InMemoryRouteHistoryRepository.cpp
    src/NavigationPersistenceTest.cpp
    src/NavigationServiceTest.cpp
    src/CalendarServiceTest.cpp
    src/ChatServiceTest.cpp
    src/CommandParserTest.cpp
    src/ConversationServiceTest.cpp
    src/DailyContextServiceTest.cpp
    src/EventServiceTest.cpp
    src/EventWarningServiceTest.cpp
    src/EveningCheckInServiceTest.cpp
    src/FakeClock.cpp
    src/FakeLlmClient.cpp
    src/GreetingServiceTest.cpp
    src/HabitServiceTest.cpp
    src/InMemoryChatRepository.cpp
    src/InMemoryEventRepository.cpp
    src/InMemoryEventWarningRepository.cpp
    src/InMemoryEveningCheckInRepository.cpp
    src/InMemoryGreetingRepository.cpp
    src/InMemoryHabitRepository.cpp
    src/InMemoryMoodRepository.cpp
    src/InMemoryReminderRepository.cpp
    src/InMemoryTaskRepository.cpp
    src/InMemoryTransactionRunner.cpp
    src/InMemoryUserProfileRepository.cpp
    src/LlmContextBuilderTest.cpp
    src/LlmClientTest.cpp
    src/MoodServiceTest.cpp
    src/PlannerRepositoriesTest.cpp
    src/PlannerTypesTest.cpp
    src/ProfileFactImportTest.cpp
    src/Pcre2RegexTest.cpp
    src/PlaceReminderTest.cpp
    src/ReminderServiceTest.cpp
    src/TaskServiceTest.cpp
    src/UserProfileServiceTest.cpp
    src/UserProfileFactSelectorTest.cpp
)

target_compile_features(msa_core_tests PRIVATE cxx_std_20)
target_include_directories(msa_core_tests PRIVATE include)
target_compile_definitions(msa_core_tests PRIVATE PCRE2_CODE_UNIT_WIDTH=8 PCRE2_STATIC)
target_link_libraries(msa_core_tests PRIVATE msa_core GTest::gtest_main pcre2-8-static)
msa_apply_warnings(msa_core_tests)

include(GoogleTest)
gtest_discover_tests(msa_core_tests)
