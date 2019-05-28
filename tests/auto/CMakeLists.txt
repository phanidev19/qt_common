set(PMI_TEST_MODULE common_ms)

add_definitions(-DPMI_TEST_FILES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

set(PMI_TEST_LINK_LIBRARIES
    #pmi_common
    #pmi_common_core
    pmi_common_ms
)

set(pmi_common_ms_TESTS
    AdvancedSettingsTest
    CrossSampleFeatureCollatorAutoTest
    CsvReaderTest
    CsvWriterTest
    FileMatrixDataStructureAutoTest
    GridUniformTest
    MS1PrefixSumTest
    MSCompareTest
    MSReaderAgilentCompareWithByspecTest
    NonUniformTileBuilderTest
    NonUniformTileFeatureFinderTest
    PlotBaseTest
    PlotLinearTransformTest
    RowNodeTest
    RowNodeUtilsTest
    ScanDataTiledIteratorTest
    ScanInfoDaoTest
    ScanIteratorTest
    TileDocumentTest
    TileFeatureFinderTest
    TimeWarpTest
    TimeWarp2DTest
    WarpCore2DTest
)

set(pmi_common_ms_REMOTE_DATA_TESTS
    ClusterFinderTest
    MSDataNonUniformTest
    MSMultiSampleTimeWarp2DTest
    MSMultiSampleTimeWarpTest
    MultiSampleScanFeatureFinderTest
    MzCalibrationTest
    VendorPathCheckerTest
)

pmi_init_test_with_remote_data(TEST ClusterFinderTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MzCalibrationTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(FILE "mzcalibrate_example.byspec2" ARGS "1")
pmi_add_test_item_with_remote_data(FILE "mzcalibrate_invalid.byspec2" ARGS "0")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSDataNonUniformTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSMultiSampleTimeWarpTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSMultiSampleTimeWarp2DTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MultiSampleScanFeatureFinderTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST VendorPathCheckerTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(RECURSIVE_DIR ".")
pmi_add_test_with_remote_data()

if(PMI_MS_ENABLE_BRUKER_API)
    list(APPEND pmi_common_ms_REMOTE_DATA_TESTS
        MSReaderBrukerTest
        MSReaderBrukerTIMSTest
    )

    pmi_init_test_with_remote_data(TEST MSReaderBrukerTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
    pmi_add_test_item_with_remote_data(DIR "Sample_1-A.1_01_985.d" ARGS "0")
    pmi_add_test_item_with_remote_data(DIR "CsI_Pos_0_G1_000003.d" ARGS "1")
    pmi_add_test_item_with_remote_data(DIR "°CsI_Pos_0_G1_000003.d" ARGS "1")
    pmi_add_test_with_remote_data()
    
    pmi_init_test_with_remote_data(TEST MSReaderBrukerTIMSTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE})
    pmi_add_test_item_with_remote_data(DIR "Sample_1-A.1_01_985.d" ARGS "0")
    pmi_add_test_item_with_remote_data(DIR "CsI_Pos_0_G1_000003.d" ARGS "1")    
    pmi_add_test_with_remote_data()
endif()

list(APPEND pmi_common_ms_REMOTE_DATA_TESTS
    MSDataNonUniformAdapterTest
    MSReaderBenchmark
    MSReaderChromatogramsTest
    MSReaderInfoTest
    MSReaderTest
    MSReaderThermoTest
    TileBuilderTest
    ZScorePeakDetectorTest
)

pmi_init_test_with_remote_data(TEST MSDataNonUniformAdapterTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSReaderBenchmark MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSReaderChromatogramsTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSReaderInfoTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSReaderTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MSReaderThermoTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(FILE "60°C_FB.raw")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST TileBuilderTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST ZScorePeakDetectorTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

if(PMI_MS_ENABLE_SHIMADZU_API)
    list(APPEND pmi_common_ms_REMOTE_DATA_TESTS
        MSReaderShimadzuTest
    )

    pmi_init_test_with_remote_data(TEST MSReaderShimadzuTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
    pmi_add_test_item_with_remote_data(FILE "BSA_Digest_1pmol-uL_DDA_003.lcd")
    pmi_add_test_with_remote_data()
endif()

if(PMI_MS_ENABLE_ABI_DISK_TO_MEMORY)
    list(APPEND pmi_common_ms_REMOTE_DATA_TESTS
        MSReaderAbiTest
    )

    pmi_init_test_with_remote_data(TEST MSReaderAbiTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "OriginalMAb")
    pmi_add_test_with_remote_data()
endif()

if(PMI_MS_ENABLE_AGILENT_API)
    list(APPEND pmi_common_ms_REMOTE_DATA_TESTS
        MSReaderAgilentTest
    )

    pmi_init_test_with_remote_data(TEST MSReaderAgilentTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d")
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "Contains_UV_Neupogen.d")
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "°Contains_UV_Neupogen.d")
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d")
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "PL-41897_Red_MT031015_06.d")
    pmi_add_test_item_with_remote_data(RECURSIVE_DIR "N44504-30-1.d")
    pmi_add_test_with_remote_data()

    if(PMI_64BIT_COMPILER)
        set(_vendor_api_arch x64)
    else()
        set(_vendor_api_arch x86)
    endif()

    set(agilent_rel_dir ${CMAKE_SOURCE_DIR}/src/vendor/vendor_api/Agilent/${_vendor_api_arch})
    file(GLOB agilent_dll_manifests RELATIVE ${agilent_rel_dir} "${agilent_rel_dir}/*.manifest")

    foreach(_file ${agilent_dll_manifests})
        add_custom_command(TARGET MSReaderAgilentTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${agilent_rel_dir}/${_file} ${PMI_BINARY_OUTPUT_DIRECTORY}
            COMMENT "Copying agilent DLL manifests to ${PMI_BINARY_OUTPUT_DIRECTORY}")
    endforeach()

    set_target_properties(MSReaderAgilentTest PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

pmi_add_tests(${pmi_common_ms_TESTS} MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

foreach(autoTestTarget ${pmi_common_ms_TESTS})
    set_property(TARGET ${autoTestTarget} PROPERTY FOLDER "Tests/pmi_common_ms/Auto")
endforeach(autoTestTarget)

foreach(testTarget ${pmi_common_ms_REMOTE_DATA_TESTS})
    set_property(TARGET ${testTarget} PROPERTY FOLDER "Tests/pmi_common_ms/Auto/")
    set(REMOTE_DATA_TARGET "${testTarget}_data")
    if(TARGET ${REMOTE_DATA_TARGET})
        set_property(TARGET ${REMOTE_DATA_TARGET} PROPERTY FOLDER "Tests/pmi_common_ms/Auto/RemoteData")
    endif()
endforeach(testTarget)

set_target_properties(MSReaderAgilentCompareWithByspecTest PROPERTIES LINK_FLAGS "/MANIFEST:NO")
