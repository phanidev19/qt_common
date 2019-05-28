pmi_set_parallel_build(OFF)

if(WIN32)
    simple_option(PMI_MS_ENABLE_BRUKER_API "Enable support for Bruker MS Reader" ON)
    simple_option(PMI_MS_ENABLE_AGILENT_API "Enable support for Agilent MS Reader" ON)
    simple_option(PMI_MS_ENABLE_ABI_DISK_TO_MEMORY "Enable support for ABSCIEX MS Reader" ON)
    simple_option(PMI_MS_ENABLE_SHIMADZU_API "Enable support for Shimadzu MS Reader" ON)
endif()

simple_option(PMI_MS_CHECK_MS_READER_SCAN_DATA "Enable validity checking of scan data by MSReader" ON)
simple_option(PMI_MS_ENABLE_CENTROID_CACHE "Enable Centroid cache" OFF)
simple_option(PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ "Enable using MS Reader for reading in MZ Calibration" ON)
simple_option(PMI_MS_OUTPUT_DEBUG_CENTROID_SMOOTHING_TO_FILE "Output Centroid smoothing to file to debug it" OFF)
simple_option(PMI_MS_PICODECOMPRESS_USE_COMPRESSINFO "Enable CompressInfo in Pico Decompress" ON)
simple_option(PMI_MS_USE_SYSTEM_CHRONO "Use system <chrono> API instead of boost::chrono" ON)

set(pmi_common_ms_LIB_SOURCES
    algo/MSMultiSampleTimeWarp.cpp
    algo/MSMultiSampleTimeWarp2D.cpp
    algo/MzCalibration.cpp
    algo/TimeWarp2D.cpp
    algo/WarpCore2D.cpp
    algo/WarpElement.cpp
    CacheFileCreatorThread.cpp
    CollateChargeClustersToFeatures.cpp
    CrossSampleFeatureCollatorTurbo.cpp
    FileMatrixDataStructure.cpp
    MS1PrefixSum.cpp
    MSReader.cpp
    MSReaderBase.cpp
    MSReaderTypes.cpp
    MSReaderInfo.cpp
    MSReaderOptions.cpp
    MSWriterByspec2.cpp
    pico/Centroid.cpp
    pico/FunctionInfoMetaDataPrinter.cpp
    pico/PicoLocalCompress.cpp
    pico/PicoLocalDecompress.cpp
    pico/PicoLocalDecompressQtUse.cpp
    pico/PicoUtil.cpp
    pico/Predictor.cpp
    pico/Reader.cpp
    pico/Sqlite.cpp
    pico/WatersCalibration.cpp
    pico/WatersMetaReading.cpp
    pmi_common_ms_debug.cpp
    ScanIterator.cpp
    tiles/CachedMSReader.cpp
    tiles/db/MetaInfoDao.cpp
    tiles/db/ScanInfoDao.cpp
    tiles/utils/ClusterFinder.cpp
    tiles/utils/FeatureClusterSqliteFormatter.cpp
    tiles/utils/HillClusterCsvFormatter.cpp
    tiles/utils/InsilicoPeptidesCsvFormatter.cpp
    tiles/utils/MSEquispacedData.cpp
    tiles/utils/NonUniformFeature.cpp
    tiles/utils/NonUniformFeatureFindingSession.cpp
    tiles/utils/NonUniformHill.cpp
    tiles/utils/NonUniformHillCluster.cpp
    tiles/utils/NonUniformHillClusterFinder.cpp
    tiles/utils/NonUniformTileFeatureFinder.cpp
    tiles/utils/NonUniformTileHillFinder.cpp
    tiles/utils/TileFeatureFinder.cpp
    tiles/utils/ZScorePeakDetector.cpp
    tiles/utils/db/FinderFeaturesDao.cpp
    tiles/utils/db/FinderSamplesDao.cpp
    tiles/MSDataIterator.cpp
    tiles/MSDataNonUniform.cpp
    tiles/MSDataNonUniformAdapter.cpp
    tiles/MSDataNonUniformGenerator.cpp
    tiles/NonUniformTileBuilder.cpp
    tiles/ResamplingIterator.cpp
    tiles/ScanDataTiledIterator.cpp
    tiles/ScanIndexNumberConverter.cpp
    tiles/ScanIndexTimeConverter.cpp
    tiles/TileBuilder.cpp
    tiles/TileDocument.cpp
    utils/MultiSampleScanFeatureFinder.cpp
    vendor/MSReaderAgilent.cpp
    vendor/MSReaderByspec.cpp
    vendor/MSReaderTesting.cpp
    vendor/MSReaderThermo.cpp
    vendor/MSReaderSimulated.cpp
    VendorPathChecker.cpp
)

set(pmi_common_ms_LIB_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_ms_export.h
    Byspec2Types.h
    CollateChargeClustersToFeatures.h
    CrossSampleFeatureCollatorTurbo.h
    FileMatrixDataStructure.h
    MS1PrefixSum.h
    MSReader.h
    MSReaderBase.h
    MSReaderTypes.h
    MSReaderInfo.h
    MSReaderInterface.h
    MSReaderOptions.h
    MSWriterByspec2.h
    ProgressBarInterface.h
    ProgressContext.h
    algo/MSMultiSampleTimeWarp.h
    algo/MSMultiSampleTimeWarp2D.h
    algo/MzCalibration.h
    algo/TimeWarp2D.h
    algo/WarpCore2D.h
    algo/WarpElement.h
    pico/Centroid.h
    pico/CompressionInfo.h
    pico/CompressionInfoHolder.h
    pico/PicoCommon.h
    pico/PicoLocalDecompress.h
    pico/PicoLocalDecompressQtUse.h
    pico/PicoUtil.h
    pico/WatersCalibration.h
    pico/WatersMetaReading.h
    ScanIterator.h
    tiles/db/MetaInfoDao.h
    tiles/db/ScanInfoDao.h
    tiles/utils/ClusterFinder.h
    tiles/utils/FeatureClusterSqliteFormatter.h
    tiles/utils/HillClusterCsvFormatter.h
    tiles/utils/InsilicoPeptidesCsvFormatter.h
    tiles/utils/MSEquispacedData.h
    tiles/utils/NonUniformFeature.h
    tiles/utils/NonUniformFeatureFindingSession.h
    tiles/utils/NonUniformHill.h
    tiles/utils/NonUniformHillCluster.h
    tiles/utils/NonUniformHillClusterFinder.h
    tiles/utils/NonUniformTileFeatureFinder.h
    tiles/utils/NonUniformTileHillFinder.h
    tiles/utils/TileFeatureFinder.h
    tiles/utils/ZScorePeakDetector.h
    tiles/utils/db/FinderFeaturesDao.h
    tiles/utils/db/FinderSamplesDao.h
    tiles/CachedMSReader.h
    tiles/MSDataIterator.h
    tiles/MSDataNonUniform.h
    tiles/MSDataNonUniformAdapter.h
    tiles/MSDataNonUniformGenerator.h
    tiles/NonUniformTileBuilder.h
    tiles/ResamplingIterator.h
    tiles/ScanDataTiledIterator.h
    tiles/ScanIndexNumberConverter.h
    tiles/ScanIndexTimeConverter.h
    tiles/TileBuilder.h
    tiles/TileDocument.h
    utils/MultiSampleScanFeatureFinder.h
    VendorPathChecker.h
)

set(pmi_common_ms_LIB_PRIVATE_HEADERS
    CacheFileCreatorThread.h
    pico/FunctionInfoMetaDataPrinter.h
    pico/PicoLocalCompress.h
    pico/Predictor.h
    pico/Reader.h
    pico/Sqlite.h
    vendor/CommonVendor.h
    vendor/MSReaderAgilent.h
    vendor/MSReaderByspec.h
    vendor/MSReaderTesting.h
    vendor/MSReaderThermo.h
    vendor/MSReaderBrukerTims.h
    vendor/MSReaderSimulated.h
)

set(pmi_common_ms_LIB_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/algo
    ${CMAKE_CURRENT_SOURCE_DIR}/pico
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/db
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles/utils/db
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PWIZ_PMI_PATH}
    ${PMIExternalsLibs_ROOT_DIR}/bin
)

if(WIN32)
    list(APPEND pmi_common_ms_LIB_SOURCES
         vendor/vendor_api/Thermo/ThermoMSFileReaderUtils.cpp
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        vendor/vendor_api/Thermo/ThermoMSFileReaderUtils.h
    )
endif()

if(PMI_MS_ENABLE_BRUKER_API)
    list(APPEND pmi_common_ms_LIB_SOURCES
        vendor/MSReaderBruker.cpp
        vendor/BrukerSample.cpp
        vendor/MSReaderBrukerTims.cpp
    )
    list(APPEND pmi_common_ms_LIB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        vendor/BrukerSample.h
        vendor/MSReaderBruker.h
        vendor/MSReaderBrukerTims.h
    )
endif()

if(PMI_MS_ENABLE_AGILENT_API)
    list(APPEND pmi_common_ms_LIB_SOURCES
        vendor/MSReaderAgilent.cpp
    )
    list(APPEND pmi_common_ms_LIB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        vendor/MSReaderAgilent.h
    )
endif()

if(PMI_MS_ENABLE_SHIMADZU_API)
    list(APPEND pmi_common_ms_LIB_SOURCES
        vendor/MSReaderShimadzu.cpp
    )
    list(APPEND pmi_common_ms_LIB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        vendor/MSReaderShimadzu.h
    )
    set(pmi_common_ms_EXTRA_LIBS ${PMIMissingAtl_LIBS})
endif()

if(PMI_MS_ENABLE_ABI_DISK_TO_MEMORY)
    list(APPEND pmi_common_ms_LIB_SOURCES
        vendor/MSReaderAbi.cpp
    )
    list(APPEND pmi_common_ms_LIB_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        vendor/MSReaderAbi.h
    )
    set(pmi_common_ms_EXTRA_LIBS ${PMIMissingAtl_LIBS})
endif()

if(PMI_MS_ENABLE_CENTROID_CACHE)
    list(APPEND pmi_common_ms_LIB_SOURCES
        algo/mscentroidcache.cpp
        algo/mscentroidcachemanager.cpp
    )
    list(APPEND pmi_common_ms_LIB_PRIVATE_HEADERS
        algo/mscentroidcache.h
        algo/mscentroidcachemanager.h
    )
endif()

if(NOT PMI_USE_CONAN)
    set(_VENDOR_APIS_ROOT ${PMIExternalsLibs_ROOT_DIR}/bin/vendor_api)

    add_library(timsdata SHARED IMPORTED)

    set(_TIMSDATA_DLL "${_VENDOR_APIS_ROOT}/BrukerTIMS/timsdata.dll")

    # TODO if(PMI_64BIT_COMPILER)
    # TODO debug/rwdi?
    set_target_properties(timsdata PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PMIExternalsLibs_INCLUDE_DIR}/vendor_api/BrukerTIMS"
        IMPORTED_IMPLIB "${PMIExternalsLibs_ROOT_DIR}/lib/vendor_api/BrukerTIMS/timsdata.lib"
        IMPORTED_LOCATION ${_TIMSDATA_DLL}
    )
endif()

add_library(pmi_common_ms SHARED ${pmi_common_ms_LIB_SOURCES})

target_link_libraries(pmi_common_ms PUBLIC pmi_common_tiles)

if(PMI_USE_CONAN)
    target_link_libraries(pmi_common_ms PRIVATE CONAN_PKG::vendor_api_abi CONAN_PKG::vendor_api_agilent CONAN_PKG::vendor_api_bruker
        CONAN_PKG::vendor_api_bruker_tims CONAN_PKG::vendor_api_thermo CONAN_PKG::vendor_api_shimadzu)       
else()
    target_link_libraries(pmi_common_ms PRIVATE timsdata)
endif()


set_property(TARGET pmi_common_ms PROPERTY FOLDER "Libs")

generate_export_header(pmi_common_ms)

pmi_set_target_headers(pmi_common_ms
    PUBLIC ${pmi_common_ms_LIB_HEADERS}
    PRIVATE ${pmi_common_ms_LIB_PRIVATE_HEADERS}
    INSTALL_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_ms
)

target_include_directories(pmi_common_ms
    PUBLIC "$<BUILD_INTERFACE:${pmi_common_ms_LIB_INCLUDE_DIRS}>"
    PRIVATE "${PMIExternalsLibs_INCLUDE_DIR}/pmi_common_ms/vendor_api/BrukerTIMS"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_ms>"
)

# Create a Config.cmake and a ConfigVersion.cmake file and install them
include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_PREFIX "lib/cmake/pmi_common_ms")
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}")
configure_package_config_file(pmi_common_msConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_msConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    #PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pmi_common_msConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_msConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_msConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    COMPONENT Devel
)
install(TARGETS pmi_common_ms
        EXPORT pmi_common_msTargets
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)
pmi_install_pdb(pmi_common_ms)
install(EXPORT pmi_common_msTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE pmi_common_msTargets.cmake
        COMPONENT Devel
)

if(NOT PMI_USE_CONAN)
    # TODO(Ivan Skiba 2018-08-23): refactor (copy-pasted from src\pmi_common_msConfig.cmake.in)
    add_custom_command(
        TARGET pmi_common_ms POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${_VENDOR_APIS_ROOT}/Thermo/ ${PMI_BINARY_OUTPUT_DIRECTORY}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${_VENDOR_APIS_ROOT}/Bruker/ ${PMI_BINARY_OUTPUT_DIRECTORY}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${_VENDOR_APIS_ROOT}/ABI/ ${PMI_BINARY_OUTPUT_DIRECTORY}/vendor_api/ABI/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${_VENDOR_APIS_ROOT}/Agilent/ ${PMI_BINARY_OUTPUT_DIRECTORY}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${_VENDOR_APIS_ROOT}/Shimadzu ${PMI_BINARY_OUTPUT_DIRECTORY}/vendor_api/Shimadzu/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_TIMSDATA_DLL} ${PMI_BINARY_OUTPUT_DIRECTORY}/
        COMMENT "Copying pmi_common_ms 3rd-party dependencies (vendor APIs) from ${_VENDOR_APIS_ROOT} to ${PMI_BINARY_OUTPUT_DIRECTORY}/"
    )

    install(DIRECTORY
        ${_VENDOR_APIS_ROOT}/Thermo/
        ${_VENDOR_APIS_ROOT}/Agilent/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
    )

    install(DIRECTORY
        ${_VENDOR_APIS_ROOT}/ABI/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/vendor_api/ABI
        COMPONENT Runtime
    )

    install(DIRECTORY
        ${_VENDOR_APIS_ROOT}/Bruker/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
    )

    install(DIRECTORY
        ${_VENDOR_APIS_ROOT}/Shimadzu/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/vendor_api/Shimadzu
        COMPONENT Runtime
    )

    install(FILES
        ${_TIMSDATA_DLL}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
    )

    unset(_VENDOR_APIS_ROOT)
    unset(_TIMSDATA_DLL)
endif()
# for MSVS
source_group("Source Files" FILES ${pmi_common_ms_LIB_SOURCES})
