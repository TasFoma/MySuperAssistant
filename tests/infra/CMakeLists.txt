add_executable(msa_infra_tests
    ../core/FakeWebSearchProvider.cpp
    SqliteNavigationRepositoryTest.cpp
    ../core/InMemoryGeocodingCacheRepository.cpp
    ExternalServicesTest.cpp
    LlamaClientTest.cpp
    SqliteChatRepositoryTest.cpp
    SqliteConversationTest.cpp
    SqliteDatabaseTest.cpp
    SqliteEveningCheckInRepositoryTest.cpp
    SqliteEventRepositoryTest.cpp
    SqliteEventWarningRepositoryTest.cpp
    SqliteGreetingRepositoryTest.cpp
    SqliteHabitRepositoryTest.cpp
    SqliteMoodRepositoryTest.cpp
    SqliteReminderRepositoryTest.cpp
    SqliteTaskRepositoryTest.cpp
    SqliteUserProfileRepositoryTest.cpp
    TestMain.cpp
    ../core/FakeClock.cpp
    ../core/FakeLlmClient.cpp
    ../core/InMemoryEventRepository.cpp
    ../core/InMemoryTaskRepository.cpp
)

target_compile_features(msa_infra_tests PRIVATE cxx_std_20)
target_link_libraries(msa_infra_tests PRIVATE msa_infra GTest::gtest)
msa_apply_warnings(msa_infra_tests)

include(GoogleTest)
gtest_add_tests(TARGET msa_infra_tests
    SOURCES SqliteDatabaseTest.cpp SqliteChatRepositoryTest.cpp SqliteUserProfileRepositoryTest.cpp
    SqliteGreetingRepositoryTest.cpp SqliteEveningCheckInRepositoryTest.cpp
    SqliteEventWarningRepositoryTest.cpp SqliteMoodRepositoryTest.cpp SqliteTaskRepositoryTest.cpp
    SqliteEventRepositoryTest.cpp SqliteReminderRepositoryTest.cpp
    SqliteHabitRepositoryTest.cpp SqliteConversationTest.cpp LlamaClientTest.cpp ExternalServicesTest.cpp
    SqliteNavigationRepositoryTest.cpp
    TEST_LIST msaInfraTests)
if(WIN32)
    set_tests_properties(${msaInfraTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
