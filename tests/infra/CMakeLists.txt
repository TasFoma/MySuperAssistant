add_executable(msa_infra_tests
    src/AudioDevicesTest.cpp
    src/MusicPlayerTest.cpp
    src/PlannerActionsTest.cpp
    src/ClientPreferencesTest.cpp
    src/ClientModelsTest.cpp
    src/TcpClientTest.cpp
    src/ProtocolTestClient.cpp
    src/ServerProcessTest.cpp
    src/ServerRuntimeTest.cpp
    src/TcpServerTest.cpp
    src/ProtocolCodecTest.cpp
    ../core/src/FakeWebSearchProvider.cpp
    src/SqliteNavigationRepositoryTest.cpp
    ../core/src/InMemoryGeocodingCacheRepository.cpp
    src/ExternalServicesTest.cpp
    src/LlamaClientTest.cpp
    src/SqliteChatRepositoryTest.cpp
    src/SqliteConversationTest.cpp
    src/SqliteDatabaseTest.cpp
    src/SqliteEveningCheckInRepositoryTest.cpp
    src/SqliteEventRepositoryTest.cpp
    src/SqliteEventWarningRepositoryTest.cpp
    src/SqliteGreetingRepositoryTest.cpp
    src/SqliteHabitRepositoryTest.cpp
    src/SqliteMoodRepositoryTest.cpp
    src/SqliteReminderRepositoryTest.cpp
    src/SqliteTaskRepositoryTest.cpp
    src/SqliteUserProfileRepositoryTest.cpp
    src/TestMain.cpp
    ../core/src/FakeClock.cpp
    ../core/src/FakeLlmClient.cpp
    ../core/src/InMemoryEventRepository.cpp
    ../core/src/InMemoryTaskRepository.cpp
)

target_compile_features(msa_infra_tests PRIVATE cxx_std_20)
target_include_directories(msa_infra_tests PRIVATE include ../core/include)
target_link_libraries(msa_infra_tests PRIVATE msa_infra msa_server_support msa_desktop_support GTest::gtest)
target_compile_definitions(msa_infra_tests PRIVATE MSA_SERVER_EXECUTABLE="$<TARGET_FILE:msa_server>")
add_dependencies(msa_infra_tests msa_server)
msa_apply_warnings(msa_infra_tests)

include(GoogleTest)
if(WIN32)
    qt_add_executable(msa_fake_voice_worker src/FakeVoiceWorker.cpp)
    target_link_libraries(msa_fake_voice_worker PRIVATE msa_infra_voice)
    target_compile_features(msa_fake_voice_worker PRIVATE cxx_std_20)
    msa_apply_warnings(msa_fake_voice_worker)
    target_sources(msa_infra_tests PRIVATE src/SpeechRecognizerTest.cpp src/VoiceModelTest.cpp)
    target_link_libraries(msa_infra_tests PRIVATE msa_infra_voice)
    target_compile_definitions(msa_infra_tests PRIVATE
        MSA_FAKE_VOICE_WORKER="$<TARGET_FILE:msa_fake_voice_worker>"
        MSA_VOICE_WORKER="$<TARGET_FILE:msa_voice_worker>"
    )
    add_dependencies(msa_infra_tests msa_fake_voice_worker msa_voice_worker)
    gtest_add_tests(TARGET msa_infra_tests SOURCES src/SpeechRecognizerTest.cpp src/VoiceModelTest.cpp TEST_LIST msaVoiceTests)
    set_tests_properties(${msaVoiceTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
gtest_add_tests(TARGET msa_infra_tests
    SOURCES src/AudioDevicesTest.cpp src/MusicPlayerTest.cpp src/PlannerActionsTest.cpp src/ClientPreferencesTest.cpp src/SqliteDatabaseTest.cpp src/SqliteChatRepositoryTest.cpp src/SqliteUserProfileRepositoryTest.cpp
    src/SqliteGreetingRepositoryTest.cpp src/SqliteEveningCheckInRepositoryTest.cpp
    src/SqliteEventWarningRepositoryTest.cpp src/SqliteMoodRepositoryTest.cpp src/SqliteTaskRepositoryTest.cpp
    src/SqliteEventRepositoryTest.cpp src/SqliteReminderRepositoryTest.cpp
    src/SqliteHabitRepositoryTest.cpp src/SqliteConversationTest.cpp src/LlamaClientTest.cpp src/ExternalServicesTest.cpp
    src/SqliteNavigationRepositoryTest.cpp src/ProtocolCodecTest.cpp src/TcpServerTest.cpp src/TcpClientTest.cpp src/ClientModelsTest.cpp src/ServerRuntimeTest.cpp src/ServerProcessTest.cpp
    TEST_LIST msaInfraTests)
if(WIN32)
    set_tests_properties(${msaInfraTests} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

add_executable(msa_ui_tests src/UiTestMain.cpp src/CommonComponentsTest.cpp src/DesktopScreensTest.cpp
    src/DesktopRuntimeTest.cpp ../core/src/FakeClock.cpp)
target_compile_features(msa_ui_tests PRIVATE cxx_std_20)
target_link_libraries(msa_ui_tests PRIVATE msa_desktop_support msa_ui_desktopplugin msa_ui_commonplugin GTest::gtest)
target_include_directories(msa_ui_tests PRIVATE ../core/include)
target_compile_definitions(msa_ui_tests PRIVATE MSA_DESKTOP_EXECUTABLE="$<TARGET_FILE:msa_desktop>"
    MSA_SERVER_EXECUTABLE="$<TARGET_FILE:msa_server>")
add_dependencies(msa_ui_tests msa_desktop msa_server)
msa_apply_warnings(msa_ui_tests)
qt_add_resources(msa_ui_tests "common_component_tests"
    PREFIX /tests
    FILES qml/CommonComponentsGallery.qml
)
gtest_add_tests(TARGET msa_ui_tests SOURCES src/CommonComponentsTest.cpp src/DesktopScreensTest.cpp src/DesktopRuntimeTest.cpp TEST_LIST msaUiTests)
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
