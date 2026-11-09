add_library(msa_ui_common STATIC
    Model/include/msa/ui/ClientListModel.h
    Model/include/msa/ui/ClientModels.h
    Model/src/ClientListModel.cpp
    Model/src/ClientModels.cpp
)

target_compile_features(msa_ui_common PUBLIC cxx_std_20)
target_include_directories(msa_ui_common PUBLIC Model/include)
target_link_libraries(msa_ui_common PUBLIC msa_core Qt6::Quick)
msa_apply_warnings(msa_ui_common)

set(msaCommonQmlFiles
    UI/Theme.qml
    UI/DetailsCard.qml
    UI/AppButton.qml
    UI/AppTextField.qml
    UI/AppTextArea.qml
    UI/Utils/SurfaceCard.qml
    UI/Utils/SectionHeader.qml
    UI/TaskRow.qml
    UI/MessageBubble.qml
    UI/HabitProgress.qml
    UI/ConnectionBanner.qml
    UI/EmptyState.qml
    UI/ErrorBanner.qml
)
foreach(qmlFile IN LISTS msaCommonQmlFiles)
    get_filename_component(qmlName "${qmlFile}" NAME)
    set_source_files_properties("${qmlFile}" PROPERTIES QT_RESOURCE_ALIAS "${qmlName}")
endforeach()
set_source_files_properties(UI/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(msa_ui_common
    URI Msa.Ui.Common
    VERSION 1.0
    CLASS_NAME MsaUiCommonPlugin
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Msa/Ui/Common"
    QML_FILES ${msaCommonQmlFiles}
)
