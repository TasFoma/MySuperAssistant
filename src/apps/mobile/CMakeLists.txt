find_package(Qt6 6.10 REQUIRED COMPONENTS Gui)

qt_add_executable(msa_mobile
    src/main.cpp
)

target_compile_features(msa_mobile PRIVATE cxx_std_20)
target_link_libraries(msa_mobile PRIVATE msa_ui_mobile msa_ui_common msa_infra msa_core Qt6::Gui)
msa_apply_warnings(msa_mobile)
