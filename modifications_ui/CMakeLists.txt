set(pmi_modifications_ui_LIB_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(pmi_modifications_ui_LIB_SOURCES
    src/CinReader.cpp
    src/FineControl.cpp
    src/GlycanEditorDialog.cpp
    src/GlycanInfoItem.cpp
    src/GlycansUiWidget.cpp
    src/GlycanInfoModel.cpp
    src/ModificationComboBox.cpp
    src/ModificationCommon.cpp
    src/ModificationInfoItem.cpp
    src/ModificationInfoModel.cpp
    src/ModificationsUiWidget.cpp
    src/MultiselectComboBox.cpp
    src/pmi_modifications_ui_debug.cpp
    src/UnimodModifications.cpp
    src/UnimodParser.cpp
    src/UnimodParserImplV2_0.cpp
)

add_library(pmi_modifications_ui SHARED ${pmi_modifications_ui_LIB_SOURCES})

target_link_libraries(pmi_modifications_ui
    PUBLIC
        Qt5::Widgets
    PRIVATE
        Qt5::Xml
)

# only for for common_errors.h
if(PMI_USE_CONAN)
    target_link_libraries(pmi_modifications_ui PUBLIC CONAN_PKG::pmi_common_stable)
else()
    target_link_libraries(pmi_modifications_ui PUBLIC pmi_common_stable)
endif()


set_property(TARGET pmi_modifications_ui PROPERTY FOLDER "Libs")

generate_export_header(pmi_modifications_ui)

pmi_set_target_headers(pmi_modifications_ui
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/pmi_modifications_ui_export.h
        src/CinReader.h
        src/GlycansUiWidget.h
        src/ModificationsUiWidget.h
    PRIVATE
        src/FineControl.h
        src/GlycanEditorDialog.h
        src/GlycanInfoItem.h
        src/GlycanInfoModel.h
        src/ModificationComboBox.h
        src/ModificationCommon.h
        src/ModificationInfoItem.h
        src/ModificationInfoModel.h
        src/MultiselectComboBox.h
        src/pmi_modifications_ui_debug.h
        src/UnimodModifications.h
        src/UnimodParser.h
        src/UnimodParserImplV2_0.h
        src/UnimodParserImplInterface.h
    INSTALL_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pmi_modifications_ui
)

target_include_directories(pmi_modifications_ui
    PUBLIC "$<BUILD_INTERFACE:${pmi_modifications_ui_LIB_INCLUDE_DIRS}>"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pmi_modifications_ui>"
)


# Create a Config.cmake and a ConfigVersion.cmake file and install them
include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_PREFIX "lib/cmake/pmi_modifications_ui")
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}")
configure_package_config_file(pmi_modifications_uiConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_modifications_uiConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pmi_modifications_uiConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_modifications_uiConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_modifications_uiConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    COMPONENT Devel
)
install(TARGETS pmi_modifications_ui
        EXPORT pmi_modifications_uiTargets
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)
pmi_install_pdb(pmi_modifications_ui)
install(EXPORT pmi_modifications_uiTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE pmi_modifications_uiTargets.cmake
        COMPONENT Devel
)


set(_UNIMOD_DATA_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data/unimod.xml")
set(_DATA_OUTPUT_DIRECTORY ${PMI_BINARY_OUTPUT_DIRECTORY}/data)
add_custom_command(
    TARGET pmi_modifications_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_DATA_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_UNIMOD_DATA_PATH} ${_DATA_OUTPUT_DIRECTORY}
    COMMENT "Copying Unimod data to ${_DATA_OUTPUT_DIRECTORY}"
)
install(FILES
    ${_UNIMOD_DATA_PATH}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/data
    COMPONENT Package
)

# for MSVS
source_group("Source Files" FILES ${pmi_modifications_ui_LIB_SOURCES})

add_subdirectory(pmi-glycansui)
add_subdirectory(pmi-modificationsui)
