set(PMI_TEST_MODULE common_tiles)

set(PMI_TEST_LINK_LIBRARIES
    # TODO not available in qt_common.git: pmi_common
    pmi_common_ms
)

set(pmi_common_tiles_TESTS
    NonUniformTilePartIteratorTest
    NonUniformTileRangeTest
    NonUniformTileStoreSqliteTest
    NonUniformTilesInfoDaoTest
    RandomBilinearTileIteratorTest
    RandomTileIteratorTest
    TileLevelSelectorTest
    TileManagerTest
    TileRangeTest
    TileStoreMemoryTest
    TileStoreSqliteTest
    TileTest
)

set(pmi_common_tiles_REMOTE_TESTS
    RandomTileIteratorBenchmark
    MzScanIndexNonUniformTileRectIteratorTest
)

pmi_init_test_with_remote_data(TEST RandomTileIteratorBenchmark MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MzScanIndexNonUniformTileRectIteratorTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_add_tests(${pmi_common_tiles_TESTS} MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

foreach(autoTestTarget ${pmi_common_tiles_TESTS})
    set_property(TARGET ${autoTestTarget} PROPERTY FOLDER "Tests/pmi_common_tiles/Auto")
endforeach(autoTestTarget)

foreach(dataTestTarget ${pmi_common_tiles_REMOTE_TESTS})
    set_property(TARGET ${dataTestTarget} PROPERTY FOLDER "Tests/pmi_common_tiles/Auto")
    set(REMOTE_DATA_TARGET "${dataTestTarget}_data")
    if(TARGET ${REMOTE_DATA_TARGET})
        set_property(TARGET ${REMOTE_DATA_TARGET} PROPERTY FOLDER "Tests/pmi_common_tiles/Auto/RemoteData")
    endif()
endforeach(dataTestTarget)
