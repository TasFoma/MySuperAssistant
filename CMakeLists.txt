cmake_minimum_required(VERSION 3.25)

project(MySuperAssistant VERSION 0.1.0 LANGUAGES CXX)

include(cmake/Warnings.cmake)

add_subdirectory(src/core)
add_subdirectory(src/infra)
add_subdirectory(src/ui/common)

if(ANDROID)
    add_subdirectory(src/ui/mobile)
    add_subdirectory(src/apps/mobile)
else()
    add_subdirectory(src/apps/server)
    add_subdirectory(src/ui/desktop)
    add_subdirectory(src/apps/desktop)
endif()