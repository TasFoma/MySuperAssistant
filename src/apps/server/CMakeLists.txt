add_library(msa_server_support STATIC
    include/msa/server/ServerSettings.h
    include/msa/server/ServerRuntime.h
    include/msa/server/NetworkServices.h
    src/ServerSettings.cpp
    src/NetworkServices.cpp
    src/ServerRuntime.cpp
)
target_compile_features(msa_server_support PUBLIC cxx_std_20)
target_include_directories(msa_server_support PUBLIC include)
target_link_libraries(msa_server_support PUBLIC msa_infra msa_core)
msa_apply_warnings(msa_server_support)

qt_add_executable(msa_server
    src/main.cpp
)

target_compile_features(msa_server PRIVATE cxx_std_20)
target_link_libraries(msa_server PRIVATE msa_server_support)
msa_apply_warnings(msa_server)
