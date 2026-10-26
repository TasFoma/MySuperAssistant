add_library(msa_infra STATIC
    src/placeholder.cpp
)

target_compile_features(msa_infra PUBLIC cxx_std_20)
target_link_libraries(msa_infra PUBLIC msa_core)
msa_apply_warnings(msa_infra)
