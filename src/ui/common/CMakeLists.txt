add_library(msa_ui_common STATIC
    include/msa/ui/ClientListModel.h
    include/msa/ui/ClientModels.h
    src/ClientListModel.cpp
    src/ClientModels.cpp
)

target_compile_features(msa_ui_common PUBLIC cxx_std_20)
target_include_directories(msa_ui_common PUBLIC include)
target_link_libraries(msa_ui_common PUBLIC msa_core Qt6::Quick)
msa_apply_warnings(msa_ui_common)
