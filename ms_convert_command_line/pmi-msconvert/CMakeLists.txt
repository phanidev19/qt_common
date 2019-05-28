include_directories(../src)

set(pmi_msconvert_SOURCES
    main.cpp
    MSConvertCommandLineParser.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2PmiCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSReaderCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/PMCommandLineParser.cpp
)

pmi_add_executable(PMi-MSConvert ${pmi_msconvert_SOURCES})
ecm_mark_nongui_executable(PMi-MSConvert)

pmi_set_target_headers(PMi-MSConvert
    PRIVATE
        MSConvertCommandLineParser.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2PmiCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSReaderCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/PMCommandLineParser.h
)

target_link_libraries(PMi-MSConvert
    pmi_common_ms
    Qt5::Core
)

pmi_add_manifest(PMi-MSConvert ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(PMi-MSConvert ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS PMi-MSConvert ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
source_group("Source Files" FILES ${pmi_msconvert_SOURCES})
set_property(TARGET PMi-MSConvert PROPERTY FOLDER "Apps")
