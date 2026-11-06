add_executable(msa_infra_tests
    SqliteChatRepositoryTest.cpp
    SqliteDatabaseTest.cpp
    SqliteEveningCheckInRepositoryTest.cpp
    SqliteGreetingRepositoryTest.cpp
    SqliteUserProfileRepositoryTest.cpp
    TestMain.cpp
    ../core/FakeClock.cpp
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
    TEST_LIST msaInfraTests)
if(WIN32)
    set_tests_properties(${msaInfraTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
