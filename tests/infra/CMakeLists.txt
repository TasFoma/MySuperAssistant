add_executable(msa_infra_tests
    ClientPreferencesTest.cpp
    ClientModelsTest.cpp
    TcpClientTest.cpp
    ProtocolTestClient.cpp
    ServerProcessTest.cpp
    ServerRuntimeTest.cpp
    TcpServerTest.cpp
    ProtocolCodecTest.cpp
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
target_link_libraries(msa_infra_tests PRIVATE msa_infra msa_server_support msa_desktop_support GTest::gtest)
target_compile_definitions(msa_infra_tests PRIVATE MSA_SERVER_EXECUTABLE="$<TARGET_FILE:msa_server>")
add_dependencies(msa_infra_tests msa_server)
msa_apply_warnings(msa_infra_tests)

include(GoogleTest)
gtest_add_tests(TARGET msa_infra_tests
    SOURCES ClientPreferencesTest.cpp SqliteDatabaseTest.cpp SqliteChatRepositoryTest.cpp SqliteUserProfileRepositoryTest.cpp
    SqliteGreetingRepositoryTest.cpp SqliteEveningCheckInRepositoryTest.cpp
    SqliteEventWarningRepositoryTest.cpp SqliteMoodRepositoryTest.cpp SqliteTaskRepositoryTest.cpp
    SqliteEventRepositoryTest.cpp SqliteReminderRepositoryTest.cpp
    SqliteHabitRepositoryTest.cpp SqliteConversationTest.cpp LlamaClientTest.cpp ExternalServicesTest.cpp
    SqliteNavigationRepositoryTest.cpp ProtocolCodecTest.cpp TcpServerTest.cpp TcpClientTest.cpp ClientModelsTest.cpp ServerRuntimeTest.cpp ServerProcessTest.cpp
    TEST_LIST msaInfraTests)
if(WIN32)
    set_tests_properties(${msaInfraTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

add_executable(msa_ui_tests UiTestMain.cpp CommonComponentsTest.cpp DesktopScreensTest.cpp)
target_compile_features(msa_ui_tests PRIVATE cxx_std_20)
target_link_libraries(msa_ui_tests PRIVATE msa_ui_desktop msa_ui_desktopplugin msa_ui_common msa_ui_commonplugin GTest::gtest)
msa_apply_warnings(msa_ui_tests)
qt_add_resources(msa_ui_tests "common_component_tests"
    PREFIX /tests
    FILES qml/CommonComponentsGallery.qml
)
gtest_add_tests(TARGET msa_ui_tests SOURCES CommonComponentsTest.cpp DesktopScreensTest.cpp TEST_LIST msaUiTests)
set_tests_properties(${msaUiTests} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
)
if(WIN32)
    set_tests_properties(${msaUiTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
    set_property(TEST ${msaUiTests} APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
endif()
