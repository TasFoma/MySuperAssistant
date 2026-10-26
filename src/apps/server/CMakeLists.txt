add_executable(msa_server
    src/main.cpp
)

target_compile_features(msa_server PRIVATE cxx_std_20)
target_link_libraries(msa_server PRIVATE msa_infra msa_core)
msa_apply_warnings(msa_server)
