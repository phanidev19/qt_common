include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/common_widgets
)

set(pmi_ms_reader_app_SOURCES
    main.cpp
    MSReaderMainWindow.cpp
)

set(pmi_ms_reader_app_HEADERS
    MSReaderMainWindow.h
)

pmi_add_executable(PMi-MSReaderGUI ${pmi_ms_reader_app_SOURCES} ${pmi_ms_reader_app_HEADERS})
pmi_add_manifest(PMi-MSReaderGUI ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(PMi-MSReaderGUI ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

target_link_libraries(PMi-MSReaderGUI
    pmi_common_ms
    pmi_common_widgets
)

set_property(TARGET PMi-MSReaderGUI PROPERTY FOLDER "Apps")

install(TARGETS PMi-MSReaderGUI
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)

# for MSVS
source_group("Source Files" FILES ${pmi_ms_reader_SOURCES})
