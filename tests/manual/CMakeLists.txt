set(PMI_TEST_MODULE common_ms)

add_definitions(-DPMI_TEST_FILES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DPMI_TEST_FILES_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")

set(PMI_TEST_LINK_LIBRARIES
    pmi_common_ms
)

#
# DbScanMsClusterTest
#

set(dbscan_ms_cluster_test_SOURCES 
    DbScanMsClusterTest.cpp
    3rd_party/dbscan.h
    3rd_party/kdtree.cpp
    3rd_party/kdtree.h
)

list(APPEND pmi_common_ms_manual_test_SOURCES ${dbscan_ms_cluster_test_SOURCES})

pmi_add_executable(DbScanMsClusterTest ${dbscan_ms_cluster_test_SOURCES})

target_link_libraries(DbScanMsClusterTest
    pmi_common_ms
)

pmi_add_manifest(DbScanMsClusterTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(DbScanMsClusterTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS DbScanMsClusterTest ${INSTALL_TARGETS_DEFAULT_ARGS})

target_include_directories(DbScanMsClusterTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/
)

ecm_mark_nongui_executable(DbScanMsClusterTest)

# for MSVS
set_property(TARGET DbScanMsClusterTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")

#
# MSReaderWebServerTest
#

find_package(QHttpEngine 0.1)

if(QHttpEngine_FOUND OR TARGET CONAN_PKG::qhttpengine)
    set(ms_reader_web_server_test_SOURCES MSReaderWebServerTest.cpp)
    list(APPEND pmi_common_ms_manual_test_SOURCES ${ms_reader_web_server_test_SOURCES})

    pmi_add_executable(MSReaderWebServerTest ${ms_reader_web_server_test_SOURCES})

    pmi_set_target_headers(MSReaderWebServerTest PRIVATE MSReaderWebServerTest.h)

    target_link_libraries(MSReaderWebServerTest pmi_common_ms)

    if(PMI_USE_CONAN)
        target_link_libraries(MSReaderWebServerTest CONAN_PKG::qhttpengine)
    else()
        target_link_libraries(MSReaderWebServerTest QHttpEngine)
    endif()

    pmi_add_manifest(MSReaderWebServerTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
    pmi_add_execonfig(MSReaderWebServerTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

    install(TARGETS MSReaderWebServerTest ${INSTALL_TARGETS_DEFAULT_ARGS})

    # for MSVS
    set_property(TARGET MSReaderWebServerTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")
endif()

# CsvParserAppTest

set(csv_parser_app_test_SOURCES CsvParserAppTest.cpp)
list(APPEND pmi_common_ms_manual_test_SOURCES ${csv_parser_app_test_SOURCES})

pmi_add_executable(CsvParserAppTest ${csv_parser_app_test_SOURCES})

target_link_libraries(CsvParserAppTest
    pmi_common_core_mini
    Qt5::Widgets
)

install(TARGETS CsvParserAppTest ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
set_property(TARGET CsvParserAppTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")

# ScanFeatureFinderTest 

set(scan_feature_finder_test_SOURCES 
    ScanFeatureFinderTest.cpp
    )

list(APPEND pmi_common_ms_manual_test_SOURCES ${scan_feature_finder_test_SOURCES})

pmi_add_executable(ScanFeatureFinderTest ${scan_feature_finder_test_SOURCES})

target_link_libraries(ScanFeatureFinderTest
    pmi_common_core_mini
    pmi_common_ms
    Qt5::Core
)

if(PMI_USE_CONAN)
    target_link_libraries(ScanFeatureFinderTest CONAN_PKG::eigen)
else()
    target_link_libraries(ScanFeatureFinderTest Eigen3::Eigen)
endif()

set_property(TARGET ScanFeatureFinderTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")

pmi_add_manifest(ScanFeatureFinderTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(ScanFeatureFinderTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

ecm_mark_nongui_executable(ScanFeatureFinderTest)


# ConstructCacheFilesBenchmark

set(construct_cache_files_benchmark_SOURCES ConstructCacheFilesBenchmark.cpp)
list(APPEND pmi_common_ms_manual_test_SOURCES ${construct_cache_files_benchmark_SOURCES})

pmi_add_executable(ConstructCacheFilesBenchmark ${construct_cache_files_benchmark_SOURCES})

ecm_mark_nongui_executable(ConstructCacheFilesBenchmark)

target_link_libraries(ConstructCacheFilesBenchmark
    pmi_common_ms
    Qt5::Core
)

pmi_add_manifest(ConstructCacheFilesBenchmark ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(ConstructCacheFilesBenchmark ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS ConstructCacheFilesBenchmark ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
set_property(TARGET ConstructCacheFilesBenchmark PROPERTY FOLDER "Tests/pmi_common_ms/Manual")




# CrossSampleFeatureCollatorManualTest

set(CrossSampleFeatureCollatorManualTest_SOURCES CrossSampleFeatureCollatorManualTest.cpp)
list(APPEND pmi_common_ms_manual_test_SOURCES ${CrossSampleFeatureCollatorManualTest_SOURCES})

pmi_add_executable(CrossSampleFeatureCollatorManualTest ${CrossSampleFeatureCollatorManualTest_SOURCES})

ecm_mark_nongui_executable(CrossSampleFeatureCollatorManualTest)

target_link_libraries(CrossSampleFeatureCollatorManualTest
    pmi_common_ms
    Qt5::Core
)

pmi_add_manifest(CrossSampleFeatureCollatorManualTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(CrossSampleFeatureCollatorManualTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS CrossSampleFeatureCollatorManualTest ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
set_property(TARGET CrossSampleFeatureCollatorManualTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")




# ShimadzuGetNumberOfSpectraCompareTest
set(ShimadzuGetNumberOfSpectraCompareTest_SOURCES ShimadzuGetNumberOfSpectraCompareTest.cpp)
list(APPEND pmi_common_ms_manual_test_SOURCES ${ShimadzuGetNumberOfSpectraCompareTest_SOURCES})
pmi_add_executable(ShimadzuGetNumberOfSpectraCompareTest ${ShimadzuGetNumberOfSpectraCompareTest_SOURCES})
ecm_mark_nongui_executable(ShimadzuGetNumberOfSpectraCompareTest)
target_link_libraries(ShimadzuGetNumberOfSpectraCompareTest
    pmi_common_core_mini
    pmi_common_ms
    Qt5::Core
)
pmi_add_manifest(ShimadzuGetNumberOfSpectraCompareTest ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(ShimadzuGetNumberOfSpectraCompareTest ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
install(TARGETS ShimadzuGetNumberOfSpectraCompareTest ${INSTALL_TARGETS_DEFAULT_ARGS})
set_property(TARGET ShimadzuGetNumberOfSpectraCompareTest PROPERTY FOLDER "Tests/pmi_common_ms/Manual")


# MSReaderManualBenchmark
set(MSReaderManualBenchmark_SOURCES MSReaderManualBenchmark.cpp)
list(APPEND pmi_common_ms_manual_test_SOURCES ${MSReaderManualBenchmark_SOURCES})
pmi_add_executable(MSReaderManualBenchmark ${MSReaderManualBenchmark_SOURCES})
ecm_mark_nongui_executable(MSReaderManualBenchmark)
target_link_libraries(MSReaderManualBenchmark
    pmi_common_core_mini
    pmi_common_ms
    Qt5::Core
)
pmi_add_manifest(MSReaderManualBenchmark ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(MSReaderManualBenchmark ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
install(TARGETS MSReaderManualBenchmark ${INSTALL_TARGETS_DEFAULT_ARGS})
set_property(TARGET MSReaderManualBenchmark PROPERTY FOLDER "Tests/pmi_common_ms/Manual")


# Misc

# for MSVS
source_group("Source Files" FILES ${pmi_common_ms_manual_test_SOURCES})
