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

set(msaCommonQmlFiles
    qml/Theme.qml
    qml/AppButton.qml
    qml/AppTextField.qml
    qml/AppTextArea.qml
    qml/SurfaceCard.qml
    qml/SectionHeader.qml
    qml/TaskRow.qml
    qml/MessageBubble.qml
    qml/HabitProgress.qml
    qml/ConnectionBanner.qml
    qml/EmptyState.qml
    qml/ErrorBanner.qml
)
foreach(qmlFile IN LISTS msaCommonQmlFiles)
    get_filename_component(qmlName "${qmlFile}" NAME)
    set_source_files_properties("${qmlFile}" PROPERTIES QT_RESOURCE_ALIAS "${qmlName}")
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(msa_ui_common
    URI Msa.Ui.Common
    VERSION 1.0
    CLASS_NAME MsaUiCommonPlugin
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Msa/Ui/Common"
    QML_FILES ${msaCommonQmlFiles}
)
