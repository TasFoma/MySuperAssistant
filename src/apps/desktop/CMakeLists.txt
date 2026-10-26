find_package(Qt6 6.10 REQUIRED COMPONENTS Gui)

qt_add_executable(msa_desktop
    src/main.cpp
)

target_compile_features(msa_desktop PRIVATE cxx_std_20)
target_link_libraries(msa_desktop PRIVATE msa_ui_desktop msa_ui_common msa_infra msa_core Qt6::Gui)
msa_apply_warnings(msa_desktop)
