add_library(msa_desktop_support STATIC
    include/msa/desktop/MusicController.h
    src/MusicController.cpp
    include/msa/desktop/DesktopPreferencesController.h
    include/msa/desktop/DesktopClientController.h
    src/DesktopPreferencesController.cpp
    src/DesktopClientController.cpp
)
target_compile_features(msa_desktop_support PUBLIC cxx_std_20)
target_include_directories(msa_desktop_support PUBLIC include)
target_link_libraries(msa_desktop_support PUBLIC msa_ui_desktop msa_ui_common msa_infra msa_infra_audio msa_core)
msa_apply_warnings(msa_desktop_support)

qt_add_executable(msa_desktop
    src/main.cpp
)

target_compile_features(msa_desktop PRIVATE cxx_std_20)
target_link_libraries(msa_desktop PRIVATE msa_desktop_support msa_ui_desktop msa_ui_commonplugin Qt6::Gui)
msa_apply_warnings(msa_desktop)
