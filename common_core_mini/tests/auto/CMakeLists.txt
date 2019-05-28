set(PMI_TEST_MODULE common_core_mini)

add_definitions(-DPMI_TEST_FILES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

set(PMI_TEST_LINK_LIBRARIES
    pmi_common_core_mini
    Qt5::Core
)

set(pmi_common_core_mini_TESTS 
    AveragineIsotopeTableTest
    BottomUpConvolutionTest
    BottomUpQueryBuilderTest
    BottomUpToIntactDataReaderTest
    CacheFileManagerTest
    ChargeDeterminatorTest
    ChargeDeterminatorNNTest
)

set(pmi_common_core_mini_REMOTE_DATA_TESTS 
    FastFileFolderHashTest
    MonoisotopeDeterminatorTest
    MonoisotopeDeterminatorNNTest
)

pmi_add_tests(${pmi_common_core_mini_TESTS} MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

pmi_init_test_with_remote_data(TEST FastFileFolderHashTest MANIFEST ${PMI_QTC_APP_MANIFEST_TEMPLATE} EXECONFIG ${PMI_QTC_APP_EXECONFIG_TEMPLATE})
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Md5" "7" "16" "7" "7" "ace53d358d1fcde8395a929adbb572a9")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Md5" "10" "64" "15" "13" "e02351ce5e8a7ecf2265c47ecbebdb01")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Sha1" "7" "16" "7" "7" "e8c68cfc5e313f6eec899632566520ba113a9397")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Sha1" "20" "64" "20" "20" "12fa3f4c11ca722762c3aeb6a89819fb36975e8e")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Sha256" "7" "16" "7" "7" "db8c97c767437361c57eb20bb385d1a5d895f48dcc789c8fc760e600a51d5cc7")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "BMS-ADC2-1000ng.d" ARGS "Sha256" "14" "32" "17" "11" "0cda56a9c9b13b82890d08b599bbe5975253746d5b3e31043ac9ab5e94ee39e7")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Md5" "7" "16" "7" "7" "8b37ad5fb0d5c552d6ae7121dc530ea8")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Md5" "18" "128" "17" "9" "56ac0e5e163076050612ac528142eea7")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Sha1" "7" "16" "7" "7" "551bc08112b86876f651bb60712e27dd6ecfe8d9")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Sha1" "19" "32" "16" "8" "db3800b534cf7acad6f0f66f6452cd4db6c96726")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Sha256" "7" "16" "7" "7" "41e23cd799b4b2573e54d308a1caea25612e5bc89f185b93e25e680f20bacae2")
pmi_add_test_item_with_remote_data(RECURSIVE_DIR "ecoli-0500-r001.d" ARGS "Sha256" "25" "64" "12" "15" "c02ad8caf6c7490b0bebc6e52cc7f5bbe408b0156e661e51021132cb450bd00e")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Md5" "7" "16" "7" "7" "f7f19f1845f6fceb10ee9a1154a1714d")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Md5" "10" "64" "15" "13" "ab47875a0fb52c5d4282fbaf5ef9375b")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Sha1" "7" "16" "7" "7" "b45a6d9a2e71f68c682800fe151f16100ffbf410")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Sha1" "20" "64" "20" "20" "c406789ebacc67858a8c8ec39914f774048eaab9")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Sha256" "7" "16" "7" "7" "1a2aa1afe53b3e4579d0097e85647092279ffea01e5117b9a4583b8b43d84df2")
pmi_add_test_item_with_remote_data(FILE "011315_DM_AvastinUS_IA_LysN.raw" ARGS "Sha256" "14" "32" "17" "11" "1522bec20a2132bb4a3c28035b5b5a4057a20093aebf868be8a89cbdbca6ee17")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Md5" "7" "16" "7" "7" "415754dfb7828df4a1ef370ca95b9bd1")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Md5" "18" "128" "17" "9" "e968945e5e0be444efa71691455dccaf")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Sha1" "7" "16" "7" "7" "d3a02efcd23745b65fd7a16259caeca00001ce21")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Sha1" "19" "32" "16" "8" "4b34218414b2fdc431601ddbdb3ea10a799b465d")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Sha256" "7" "16" "7" "7" "77d23d11bc695887c5be88a195a59b1a9b4b463405e307b1120b44808f7c8e27")
pmi_add_test_item_with_remote_data(FILE "020215_DM_Av.byspec2" ARGS "Sha256" "25" "64" "12" "15" "fa7317330c49ba293784f5e19f1bba888686add3b74b7de92447c54379ee899a")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Md5" "7" "16" "7" "7" "571c68b4f826c869825478eec66d7dda")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Md5" "10" "64" "15" "13" "571c68b4f826c869825478eec66d7dda")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Sha1" "7" "16" "7" "7" "84ec090c218fc71fe2bcbb9220ef90b805e9c294")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Sha1" "20" "64" "20" "20" "84ec090c218fc71fe2bcbb9220ef90b805e9c294")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Sha256" "7" "16" "7" "7" "5abf144e85a224cacd21f166fe7755e4da9e269f2d1f940c1200b0dcbc739f2d")
pmi_add_test_item_with_remote_data(FILE "BinPump1.cg" ARGS "Sha256" "14" "32" "17" "11" "5abf144e85a224cacd21f166fe7755e4da9e269f2d1f940c1200b0dcbc739f2d")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Md5" "7" "16" "7" "7" "7d61946eb86f4f28eb62fe0f07f2a120")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Md5" "18" "128" "17" "9" "7d61946eb86f4f28eb62fe0f07f2a120")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Sha1" "7" "16" "7" "7" "f8d2bc1f9c152d40520c527454bd510a7de92475")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Sha1" "19" "32" "16" "8" "f8d2bc1f9c152d40520c527454bd510a7de92475")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Sha256" "7" "16" "7" "7" "b161ffb05c6c63b94be0d85c55d56b3aec1928434b51292881efc07c735671e7")
pmi_add_test_item_with_remote_data(FILE "MSMassCal.bin" ARGS "Sha256" "25" "64" "12" "15" "b161ffb05c6c63b94be0d85c55d56b3aec1928434b51292881efc07c735671e7")
pmi_add_test_item_with_remote_data(DIR "LT-1223_test_1" ARGS "Sha1" "10" "64" "10" "10" "3e265f2ac9dc9fc42643cf6c1fbdf3a71d10e61a")
pmi_add_test_item_with_remote_data(DIR "LT-1223_test_2" ARGS "Sha1" "10" "64" "10" "10" "49a1ed9d4912996416a7be1d08baf19675510b41")
pmi_add_test_item_with_remote_data(DIR "LT-1223_test_3" ARGS "Sha1" "10" "64" "10" "10" "1960c16c271c8b0a2676519f6107cbc964cc0695")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MonoisotopeDeterminatorTest)
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

pmi_init_test_with_remote_data(TEST MonoisotopeDeterminatorNNTest)
pmi_add_test_item_with_remote_data(DIR "remoteData")
pmi_add_test_with_remote_data()

# for MSVS
foreach(testTarget ${pmi_common_core_mini_TESTS})
    set_property(TARGET ${testTarget} PROPERTY FOLDER "Tests/pmi_common_core_mini/Auto")
endforeach()

foreach(remoteDataTestTarget ${pmi_common_core_mini_REMOTE_DATA_TESTS})
    set_property(TARGET ${remoteDataTestTarget} PROPERTY FOLDER "Tests/pmi_common_core_mini/Auto")
    set(REMOTE_DATA_TARGET "${remoteDataTestTarget}_data")
    if(TARGET ${REMOTE_DATA_TARGET})
        set_property(TARGET ${REMOTE_DATA_TARGET} PROPERTY FOLDER "Tests/pmi_common_core_mini/Auto/RemoteData")
    endif()
endforeach()
