add_executable(msa_core_tests
    CalendarServiceTest.cpp
    ChatServiceTest.cpp
    CommandParserTest.cpp
    DailyContextServiceTest.cpp
    EventServiceTest.cpp
    EventWarningServiceTest.cpp
    EveningCheckInServiceTest.cpp
    FakeClock.cpp
    FakeLlmClient.cpp
    GreetingServiceTest.cpp
    HabitServiceTest.cpp
    InMemoryChatRepository.cpp
    InMemoryEventRepository.cpp
    InMemoryEventWarningRepository.cpp
    InMemoryEveningCheckInRepository.cpp
    InMemoryGreetingRepository.cpp
    InMemoryHabitRepository.cpp
    InMemoryMoodRepository.cpp
    InMemoryReminderRepository.cpp
    InMemoryTaskRepository.cpp
    InMemoryUserProfileRepository.cpp
    LlmContextBuilderTest.cpp
    LlmClientTest.cpp
    MoodServiceTest.cpp
    PlannerRepositoriesTest.cpp
    PlannerTypesTest.cpp
    Pcre2RegexTest.cpp
    PlaceReminderTest.cpp
    ReminderServiceTest.cpp
    TaskServiceTest.cpp
    UserProfileServiceTest.cpp
    UserProfileFactSelectorTest.cpp
)

target_compile_features(msa_core_tests PRIVATE cxx_std_20)
target_include_directories(msa_core_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core/src)
target_compile_definitions(msa_core_tests PRIVATE PCRE2_CODE_UNIT_WIDTH=8 PCRE2_STATIC)
target_link_libraries(msa_core_tests PRIVATE msa_core GTest::gtest_main pcre2-8-static)
msa_apply_warnings(msa_core_tests)

include(GoogleTest)
gtest_discover_tests(msa_core_tests)
