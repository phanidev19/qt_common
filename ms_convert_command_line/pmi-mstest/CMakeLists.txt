include_directories(../src)

set(pmi_mstest_SOURCES
    main.cpp
    MSCommandLineReader.cpp
    MSTestCommandLineParser.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonStructs.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonWriter.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ExtractCalibrationPointsCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ExtractMassIntensityCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2Command.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2PmiCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSComparisonManager.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSReaderCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/NonUniformCacheCommand.cpp
    ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/PMCommandLineParser.cpp
)

pmi_add_executable(PMi-MSTest ${pmi_mstest_SOURCES})
ecm_mark_nongui_executable(PMi-MSTest)

pmi_set_target_headers(PMi-MSTest
    PRIVATE
        MSCommandLineReader.h
        MSTestCommandLineParser.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonStructs.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ComparisonWriter.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ExtractCalibrationPointsCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/ExtractMassIntensityCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2Command.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MakeByspec2PmiCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSComparisonManager.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/MSReaderCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/NonUniformCacheCommand.h
        ${MS_CONVERT_COMMAND_LINE_COMMON_DIR}/PMCommandLineParser.h
)

target_link_libraries(PMi-MSTest
    pmi_common_ms
    Qt5::Core
)

pmi_add_manifest(PMi-MSTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(PMi-MSTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS PMi-MSTest ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
source_group("Source Files" FILES ${pmi_mstest_SOURCES})
set_property(TARGET PMi-MSTest PROPERTY FOLDER "Apps")
