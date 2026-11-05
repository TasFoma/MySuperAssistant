include(${PROJECT_SOURCE_DIR}/cmake/Pcre2.cmake)

add_library(msa_core STATIC
    include/msa/core/Command.h
    include/msa/core/CommandParser.h
    include/msa/core/Event.h
    include/msa/core/GeoCoordinates.h
    include/msa/core/Habit.h
    include/msa/core/HabitLogEntry.h
    include/msa/core/Reminder.h
    include/msa/core/Task.h
    include/msa/core/UserProfile.h
    src/CommandParser.cpp
    src/Pcre2Regex.cpp
    src/Pcre2Regex.h
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
