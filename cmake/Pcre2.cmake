# PCRE2 для core: регулярные выражения с UTF и Unicode-свойствами, как QRegularExpression.
# Подключается из src/core/CMakeLists.txt, поэтому работает и на ПК, и на Android.
# Решение 11 в docs/decisions.md.

include(FetchContent)

# Только 8-битная библиотека, без утилит, тестов и JIT (JIT на Android/Windows не нужен).
set(PCRE2_BUILD_PCRE2_8 ON CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2_16 OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2_32 OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2TEST OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_JIT OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_UNICODE ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    pcre2
    GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
    GIT_TAG pcre2-10.47
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(pcre2)
