add_library(msa_core STATIC
    src/placeholder.cpp
)

target_compile_features(msa_core PUBLIC cxx_std_20)
msa_apply_warnings(msa_core)
