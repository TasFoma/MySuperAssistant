add_library(msa_ui_desktop STATIC
    Model/src/placeholder.cpp
    Model/include/msa/ui/DesktopState.h
    Model/src/DesktopState.cpp
    Model/include/msa/ui/PlannerEditor.h
    Model/src/PlannerEditor.cpp
)

target_compile_features(msa_ui_desktop PUBLIC cxx_std_20)
target_include_directories(msa_ui_desktop PUBLIC Model/include)
target_link_libraries(msa_ui_desktop PUBLIC msa_ui_common Qt6::Quick)
msa_apply_warnings(msa_ui_desktop)

set(msaDesktopQmlFiles
    UI/DesktopWindow.qml
    UI/DesktopShell.qml
    UI/PlannerDialog.qml
    UI/EventRecord.qml
    UI/ChatScreen.qml
    UI/PlansScreen.qml
    UI/RemindersScreen.qml
    UI/SettingsScreen.qml
)
foreach(qmlFile IN LISTS msaDesktopQmlFiles)
    get_filename_component(qmlName "${qmlFile}" NAME)
    set_source_files_properties("${qmlFile}" PROPERTIES QT_RESOURCE_ALIAS "${qmlName}")
endforeach()
qt_add_qml_module(msa_ui_desktop
    URI Msa.Ui.Desktop
    VERSION 1.0
    CLASS_NAME MsaUiDesktopPlugin
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Msa/Ui/Desktop"
    IMPORTS Msa.Ui.Common
    QML_FILES ${msaDesktopQmlFiles}
)
