# Внешние зависимости проекта. Подключается только для сборки на ПК (см. корневой CMakeLists.txt).
# Пакетный менеджер не используем: решение 10 в docs/decisions.md.

include(FetchContent)

# Не собираем установщик GoogleTest и используем общую CRT, иначе на MSVC возможен конфликт.
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)
