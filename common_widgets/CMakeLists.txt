set(pmi_common_widgets_LIB_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set(pmi_common_widgets_SOURCES   
    MSReaderTableModel.cpp
    MSReaderTableView.cpp
    MSReaderWidget.cpp
    MSXICDataDialog.cpp
    MSChromatogramDialog.cpp
)

set(pmi_common_widgets_HEADERS
    MSReaderTableModel.h
    MSReaderTableView.h
    MSReaderWidget.h
    MSXICDataDialog.h
    MSChromatogramDialog.h
)

add_library(pmi_common_widgets SHARED ${pmi_common_widgets_SOURCES})

target_link_libraries(pmi_common_widgets PUBLIC pmi_common_ms)

if(PMI_USE_CONAN)
    target_link_libraries(pmi_common_widgets PUBLIC CONAN_PKG::qwt)
else()
    target_link_libraries(pmi_common_widgets PUBLIC Qwt)
endif()

set_property(TARGET pmi_common_widgets PROPERTY FOLDER "Libs")

generate_export_header(pmi_common_widgets)

pmi_set_target_headers(pmi_common_widgets
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_widgets_export.h
        MSReaderWidget.h
    INSTALL_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_widgets
)

target_include_directories(pmi_common_widgets
    PUBLIC "$<BUILD_INTERFACE:${pmi_common_widgets_LIB_INCLUDE_DIRS}>"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_widgets>"
)


# Create a Config.cmake and a ConfigVersion.cmake file and install them
include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_PREFIX "lib/cmake/pmi_common_widgets")
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}")
configure_package_config_file(pmi_common_widgetsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_widgetsConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pmi_common_widgetsConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_widgetsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_widgetsConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    COMPONENT Devel
)
install(TARGETS pmi_common_widgets
        EXPORT pmi_common_widgetsTargets
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)
pmi_install_pdb(pmi_common_widgets)
install(EXPORT pmi_common_widgetsTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE pmi_common_widgetsTargets.cmake
        COMPONENT Devel
)

# for MSVS
source_group("Source Files" FILES ${pmi_common_widgets_SOURCES})
