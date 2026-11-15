add_library(msa_ui_mobile STATIC
    src/placeholder.cpp
    Model/include/msa/ui/MobileState.h
    Model/src/MobileState.cpp
)

target_compile_features(msa_ui_mobile PUBLIC cxx_std_20)
target_include_directories(msa_ui_mobile PUBLIC Model/include)
target_link_libraries(msa_ui_mobile PUBLIC msa_ui_common Qt6::Quick)
msa_apply_warnings(msa_ui_mobile)

set(msaMobileQmlFiles
    UI/MobileWindow.qml
    UI/MobileShell.qml
    UI/MobileChatScreen.qml
    UI/MobilePlansScreen.qml
    UI/MobileRemindersScreen.qml
    UI/MobileSettingsScreen.qml
)
foreach(qmlFile IN LISTS msaMobileQmlFiles)
    get_filename_component(qmlName "${qmlFile}" NAME)
    set_source_files_properties("${qmlFile}" PROPERTIES QT_RESOURCE_ALIAS "${qmlName}")
endforeach()
qt_add_qml_module(msa_ui_mobile
    URI Msa.Ui.Mobile
    VERSION 1.0
    CLASS_NAME MsaUiMobilePlugin
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Msa/Ui/Mobile"
    IMPORTS Msa.Ui.Common
    QML_FILES ${msaMobileQmlFiles}
)
