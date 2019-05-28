# FastFileFolderHashBenchmark

set(fast_file_folder_hash_benchmark_SOURCES FastFileFolderHashBenchmark.cpp)
list(APPEND pmi_common_core_mini_manual_test_SOURCES ${fast_file_folder_hash_benchmark_SOURCES})

pmi_add_executable(FastFileFolderHashBenchmark ${fast_file_folder_hash_benchmark_SOURCES})
ecm_mark_nongui_executable(FastFileFolderHashBenchmark)

target_link_libraries(FastFileFolderHashBenchmark
    pmi_common_core_mini
    Qt5::Core
)

install(TARGETS FastFileFolderHashBenchmark ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
set_property(TARGET FastFileFolderHashBenchmark PROPERTY FOLDER "Tests/pmi_common_core_mini/Manual")




# BottomUpCSVReaderTest

set(bottom_up_csv_reader_SOURCES BottomUpCSVReaderTest.cpp)
list(APPEND pmi_common_core_mini_manual_test_SOURCES ${bottom_up_csv_reader_SOURCES})

pmi_add_executable(BottomUpCSVReaderTest ${bottom_up_csv_reader_SOURCES})
ecm_mark_nongui_executable(BottomUpCSVReaderTest)

target_link_libraries(BottomUpCSVReaderTest
    pmi_common_core_mini
    Qt5::Core
)

install(TARGETS BottomUpCSVReaderTest ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
set_property(TARGET BottomUpCSVReaderTest PROPERTY FOLDER "Tests/pmi_common_core_mini/Manual")



# Misc

# for MSVS
source_group("Source Files" FILES ${pmi_common_core_mini_manual_test_SOURCES})

