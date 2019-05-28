add_definitions(-DPMI_TEST_FILES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

pmi_add_test_subdirectory(auto)

pmi_add_test_subdirectory(manual)
