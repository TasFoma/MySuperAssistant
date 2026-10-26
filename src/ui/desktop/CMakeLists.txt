find_package(Qt6 6.10 REQUIRED COMPONENTS Quick)

add_library(msa_ui_desktop STATIC
    src/placeholder.cpp
)

target_compile_features(msa_ui_desktop PUBLIC cxx_std_20)
target_link_libraries(msa_ui_desktop PUBLIC msa_ui_common Qt6::Quick)
msa_apply_warnings(msa_ui_desktop)
