include(${PROJECT_SOURCE_DIR}/cmake/Pcre2.cmake)

add_library(msa_core STATIC
    include/msa/core/CalendarService.h
    include/msa/core/Clock.h
    include/msa/core/Command.h
    include/msa/core/CommandParser.h
    include/msa/core/DailyContextService.h
    include/msa/core/Event.h
    include/msa/core/EventRepository.h
    include/msa/core/EventService.h
    include/msa/core/EventWarningRepository.h
    include/msa/core/EventWarningService.h
    include/msa/core/GeoCoordinates.h
    include/msa/core/GreetingRepository.h
    include/msa/core/GreetingService.h
    include/msa/core/Habit.h
    include/msa/core/HabitLogEntry.h
    include/msa/core/HabitRepository.h
    include/msa/core/HabitService.h
    include/msa/core/Reminder.h
    include/msa/core/ReminderRepository.h
    include/msa/core/ReminderService.h
    include/msa/core/Task.h
    include/msa/core/TaskRepository.h
    include/msa/core/TaskService.h
    include/msa/core/UserProfile.h
    include/msa/core/UserProfileRepository.h
    include/msa/core/UserProfileService.h
    src/CalendarService.cpp
    src/Clock.cpp
    src/CommandParser.cpp
    src/DailyContextService.cpp
    src/EventRepository.cpp
    src/EventService.cpp
    src/EventWarningRepository.cpp
    src/EventWarningService.cpp
    src/GreetingRepository.cpp
    src/GreetingService.cpp
    src/HabitRepository.cpp
    src/HabitService.cpp
    src/Pcre2Regex.cpp
    src/Pcre2Regex.h
    src/PlannerText.cpp
    src/PlannerText.h
    src/ReminderRepository.cpp
    src/ReminderService.cpp
    src/TaskRepository.cpp
    src/TaskService.cpp
    src/UserProfileRepository.cpp
    src/UserProfileService.cpp
    src/Utf8Text.cpp
    src/Utf8Text.h
)

target_include_directories(msa_core
    PUBLIC include
    PRIVATE src
)

target_compile_features(msa_core PUBLIC cxx_std_20)
target_link_libraries(msa_core PRIVATE pcre2-8-static)
target_compile_definitions(msa_core PRIVATE PCRE2_CODE_UNIT_WIDTH=8 PCRE2_STATIC)
target_compile_options(msa_core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
msa_apply_warnings(msa_core)
