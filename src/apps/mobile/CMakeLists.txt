qt_policy(SET QTP0002 NEW)

add_library(msa_mobile_support STATIC
    include/msa/mobile/MobileRuntime.h
    src/MobileRuntime.cpp
)
target_compile_features(msa_mobile_support PUBLIC cxx_std_20)
target_include_directories(msa_mobile_support PUBLIC include)
target_link_libraries(msa_mobile_support PUBLIC msa_client_support msa_ui_mobile)
msa_apply_warnings(msa_mobile_support)

qt_add_executable(msa_mobile
    src/main.cpp
)

target_compile_features(msa_mobile PRIVATE cxx_std_20)
target_link_libraries(msa_mobile PRIVATE msa_mobile_support msa_ui_mobileplugin msa_ui_commonplugin Qt6::Gui)
set_property(TARGET msa_mobile APPEND PROPERTY QT_QML_IMPORT_PATH
    "${PROJECT_BINARY_DIR}/src/ui/common"
    "${PROJECT_BINARY_DIR}/src/ui/mobile"
)
msa_apply_warnings(msa_mobile)
