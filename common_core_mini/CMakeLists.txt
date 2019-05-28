set(CMAKE_AUTOUIC OFF) # in-convenience

set(pmi_common_core_mini_LIB_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/algo
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cache_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/calibration
    ${CMAKE_CURRENT_SOURCE_DIR}/src/csv
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hash
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

set(pmi_common_core_mini_LIB_SOURCES
    src/AdvancedSettings.cpp
    src/CustomSettings.cpp
    src/CustomSettingsBase.cpp
    src/PmiQtCommonConstants.cpp
    src/algo/AbstractChargeDeterminator.cpp
    src/algo/AbstractMonoisotopeDeterminator.cpp
    src/algo/AveragineGenerator.cpp
    src/algo/AveragineIsotopeTable.cpp
    src/algo/BottomUpConvolution.cpp
    src/algo/BottomUpQueryBuilder.cpp
    src/algo/BottomUpToIntactDataReader.cpp
    src/algo/ByophysicalAAParameters.cpp
    src/algo/CentroidOptions.cpp
    src/algo/ChargeDeterminator.cpp
    src/algo/ChargeDeterminatorNN.cpp
    src/algo/ClusteringDBSCAN.cpp
    src/algo/CommonFunctions.cpp
    src/algo/EigenUtilities.cpp
    src/algo/FauxScanCreator.cpp
    src/algo/FauxSpectraReader.cpp
    src/algo/FindMzToProcess.cpp
    src/algo/GridUniform.cpp
    src/algo/InsilicoGenerator.cpp
    src/algo/MonoisotopeDeterminator.cpp
    src/algo/MonoisotopeDeterminatorNN.cpp
    src/algo/MzCalibrationOptions.cpp
    src/algo/PQMFeatureMatcher.cpp
    src/algo/SpectraDisambigutron.cpp
    src/algo/SpectraSubtractomatic.cpp
    src/algo/TimeWarp.cpp
    src/db/EntryDefMap.cpp
    src/db/RowNode.cpp
    src/db/RowNodeOptionsCore.cpp
    src/db/RowNodeUtils.cpp
    src/db/ScopedQSqlDatabase.cpp
    src/db/common/PlotBase.cpp
    src/db/common/PlotBaseUtils.cpp
    src/db/common/Point2dListUtils.cpp
    src/hash/FastFileFolderHash.cpp
    src/hash/HashUtils.cpp
    src/logger/LogStreamRedirector_p.cpp
    src/logger/PmiLogger.cpp
    src/math/Convolution1dCore.cpp
    src/math/CosineCorrelator.cpp
    src/math/Interval.cpp
    src/math/VectorStats.cpp
    src/pmi_common_core_mini_debug.cpp
    src/sdk/TestArgumentProcessor.cpp
    src/sdk/TestImageUtils.cpp
    src/cache_files/CacheFileManager.cpp
    src/utils/ComInitializer.cpp
    src/utils/DateTimeUtils.cpp
    src/utils/DefaultPlotsColors.cpp
    src/utils/FilePathUtils.cpp
    src/utils/PathUtils.cpp
    src/utils/PmiMemoryInfo.cpp
    src/csv/CsvWriter.cpp
    src/csv/CsvReader.cpp
)

set (pmi_common_core_mini_RESOURCES
    resources/common_core_mini.qrc
)

qt5_add_resources(pmi_common_core_mini_LIB_SOURCES
    ${pmi_common_core_mini_RESOURCES}
)

set (pmi_common_core_mini_LIB_FORMS
    src/logger/PmiLogger.ui
)

qt5_wrap_ui(pmi_common_core_mini_LIB_SOURCES
    ${pmi_common_core_mini_LIB_FORMS}
)

add_library(pmi_common_core_mini SHARED ${pmi_common_core_mini_LIB_SOURCES})

target_link_libraries(pmi_common_core_mini
    PUBLIC
        Qt5::Gui # QVector2D
        Qt5::Network
        Qt5::PrintSupport
        Qt5::Svg
        Qt5::Xml
        psapi # GetProcessMemoryInfo()
)

if(PMI_USE_CONAN)
    target_link_libraries(pmi_common_core_mini PUBLIC CONAN_PKG::eigen CONAN_PKG::pmi_common_stable)
    target_link_libraries(pmi_common_core_mini PUBLIC Qt5::Sql)
else()
    target_link_libraries(pmi_common_core_mini PUBLIC Eigen3::Eigen pmi_common_stable )
endif()

set_property(TARGET pmi_common_core_mini PROPERTY FOLDER "Libs")

generate_export_header(pmi_common_core_mini)

pmi_set_target_headers(pmi_common_core_mini
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_core_mini_export.h
        src/AdvancedSettings.h
        src/CustomSettings.h
        src/CustomSettingsBase.h
        src/PmiQtCommonConstants.h
        src/algo/AbstractChargeDeterminator.h
        src/algo/AbstractMonoisotopeDeterminator.h
        src/algo/AveragineGenerator.h
        src/algo/AveragineIsotopeTable.h
        src/algo/BottomUpConvolution.h
        src/algo/BottomUpQueryBuilder.h
        src/algo/BottomUpToIntactDataReader.h
        src/algo/ByophysicalAAParameters.h
        src/algo/CentroidOptions.h
        src/algo/ChargeDeterminator.h
        src/algo/ChargeDeterminatorNN.h
        src/algo/ClusteringDBSCAN.h
        src/algo/CommonFunctions.h
        src/algo/EigenUtilities.h
        src/algo/FauxScanCreator.h
        src/algo/FauxSpectraReader.h
        src/algo/FeatureFinderParameters.h
        src/algo/FindMzToProcess.h
        src/algo/GridUniform.h
        src/algo/InsilicoGenerator.h
        src/algo/MonoisotopeDeterminator.h
        src/algo/MonoisotopeDeterminatorNN.h
        src/algo/MzCalibrationOptions.h
        # Note: nanoflann.hpp is 3rd party code
        src/algo/nanoflann.hpp
        src/algo/PQMFeatureMatcher.h
        src/algo/SpectraDisambigutron.h
        src/algo/SpectraSubtractomatic.h
        src/algo/TimeWarp.h
        src/calibration/Calibration.h
        src/db/common/PlotBase.h
        src/db/common/PlotBaseUtils.h
        src/db/common/Point2dListUtils.h
        src/db/common/PointListUtils.h
        src/db/EntryDefMap.h
        src/db/QtSqlRowNodeUtils.h
        src/db/RowNode.h
        src/db/RowNodeOptionsCore.h
        src/db/RowNodeUtils.h
        src/db/ScopedQSqlDatabase.h
        src/Global.h
        src/hash/FastFileFolderHash.h
        src/hash/HashUtils.h
        src/logger/PmiLogger.h
        src/math/Convolution1dCore.h
        src/math/CosineCorrelator.h
        src/math/Interval.h
        src/math/VectorStats.h
        src/math/MzInterval.h
        src/pmi_common_core_mini_debug.h
        src/PMiTestUtils.h
        src/sdk/TestArgumentProcessor.h
        src/sdk/TestImageUtils.h
        src/cache_files/CacheFileManager.h
        src/cache_files/CacheFileManagerInterface.h
        src/utils/ComInitializer.h
        src/utils/DateTimeUtils.h
        src/utils/DefaultPlotsColors.h
        src/utils/FilePathUtils.h
        src/utils/MathUtils.h
        src/utils/PathUtils.h
        src/utils/PmiMemoryInfo.h
        src/WorkflowLogs.h
        src/WorkflowLogs.h
        src/csv/CsvWriter.h
        src/csv/CsvReader.h
    PRIVATE
        src/logger/LogStreamRedirector_p.h
        src/logger/PmiLoggerInstance_p.h
    INSTALL_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_core_mini
)

target_include_directories(pmi_common_core_mini
    PUBLIC "$<BUILD_INTERFACE:${pmi_common_core_mini_LIB_INCLUDE_DIRS}>"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_core_mini>"
)

# Create a Config.cmake and a ConfigVersion.cmake file and install them
include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_PREFIX "lib/cmake/pmi_common_core_mini")
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}")
configure_package_config_file(pmi_common_core_miniConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_core_miniConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pmi_common_core_miniConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_core_miniConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_core_miniConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    COMPONENT Devel
)
install(TARGETS pmi_common_core_mini
        EXPORT pmi_common_core_miniTargets
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)
pmi_install_pdb(pmi_common_core_mini)
install(EXPORT pmi_common_core_miniTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE pmi_common_core_miniTargets.cmake
        COMPONENT Devel
)

add_subdirectory(tests)

# install and copy the neural network sqlite db
set(_NN_WEIGHTS_DB_BINDIR ${CMAKE_INSTALL_BINDIR})
if(PMI_USE_CONAN)
    set(NN_WEIGHTS_DB_PATH "${PROJECT_SOURCE_DIR}/data/featureFinder/nn_weights.db")
else()
    set(NN_WEIGHTS_DB_PATH "${PMI_EXTERNALS_LIBS_PATH}/data/featureFinder/nn_weights.db")
endif()

add_custom_command(
    TARGET pmi_common_core_mini POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NN_WEIGHTS_DB_PATH} ${PMI_BINARY_OUTPUT_DIRECTORY}
    COMMENT "Copying pmi_common_core_mini Neural Network to ${PMI_BINARY_OUTPUT_DIRECTORY}/"
)

install(FILES ${NN_WEIGHTS_DB_PATH}
        DESTINATION ${_NN_WEIGHTS_DB_BINDIR}
        COMPONENT Runtime
)

# for MSVS
source_group("Source Files" FILES ${pmi_common_core_mini_LIB_SOURCES})
