# Набор предупреждений для целей проекта. Вызывается из CMakeLists.txt каждой цели.

function(msa_apply_warnings targetName)
    if(MSVC)
        target_compile_options(${targetName} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${targetName} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
