add_library(msa_client_support STATIC
    include/msa/client/SportController.h
    src/SportController.cpp
    src/SportWorkoutFlow.cpp
    include/msa/client/ClientController.h
    src/ClientController.cpp
)
target_compile_features(msa_client_support PUBLIC cxx_std_20)
target_include_directories(msa_client_support PUBLIC include)
target_link_libraries(msa_client_support PUBLIC msa_ui_common msa_infra msa_core)
msa_apply_warnings(msa_client_support)
