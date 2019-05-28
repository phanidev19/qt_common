# Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
# Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
# Confidential.
#

# An option to use conan or PEL
# This will be used in transition period.
# At some point, PEL will be deprecated and we will use only conan


#https://docs.conan.io/en/latest/integrations/cmake/cmake_multi_generator.html
if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    set(_PMI_USE_CONAN ON)
elseif(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    set(_PMI_USE_CONAN ON)
else()
    set(_PMI_USE_CONAN OFF)
endif()

set(PMI_USE_CONAN ${_PMI_USE_CONAN} CACHE BOOL "Use conan to download dependencies(if OFF PEL will be used)")
unset(_PMI_USE_CONAN)

if(PMI_USE_CONAN)
    conan_basic_setup(TARGETS)

    # for cmake multi we need to set CMAKE_PREFIX_PATH and CMAKE_MODULE_PATH
    # This can be removed if we set a default value for CMAKE_BUILD_TYPE
    if(NOT CONAN_PEL_SCRIPTS_ROOT)
        set(CMAKE_PREFIX_PATH ${CONAN_CMAKE_MODULE_PATH_DEBUG} ${CONAN_CMAKE_MODULE_PATH_RELEASE} ${CMAKE_PREFIX_PATH})
        set(CMAKE_MODULE_PATH ${CONAN_CMAKE_MODULE_PATH_DEBUG} ${CONAN_CMAKE_MODULE_PATH_RELEASE} ${CMAKE_MODULE_PATH})
    endif()
else()
    # Find pmi_externals_libs path
    #
    # Tries to find pmi_externals_libs root source directory (required).
    # Defaults to ${PROJECT_SOURCE_DIR}/../pmi_externals_libs.
    # Results:
    # - sets PROJECT_PARENT_SOURCE_DIR to ${PROJECT_SOURCE_DIR}/..
    # - sets PMI_EXTERNALS_LIBS_PATH cached path
    # - adds ${PMI_EXTERNALS_LIBS_PATH}/cmake/modules to CMAKE_MODULE_PATH

    get_filename_component(PROJECT_PARENT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/.." REALPATH CACHE)
    if(PMI_EXTERNALS_LIBS_PATH)
        set(_CURRENT_PMI_EXTERNALS_LIBS_PATH "${PMI_EXTERNALS_LIBS_PATH}")
    else()
        set(_CURRENT_PMI_EXTERNALS_LIBS_PATH "${PROJECT_PARENT_SOURCE_DIR}/pmi_externals_libs_new")
        message(STATUS "PMI_EXTERNALS_LIBS_PATH was not set; defaulting to ${_CURRENT_PMI_EXTERNALS_LIBS_PATH}")
    endif()

    file(TO_CMAKE_PATH "${_CURRENT_PMI_EXTERNALS_LIBS_PATH}" _CURRENT_PMI_EXTERNALS_LIBS_PATH)
    set(PMI_EXTERNALS_LIBS_PATH "${_CURRENT_PMI_EXTERNALS_LIBS_PATH}"
                                CACHE PATH "External PMi libraries' root directory")
    unset(_CURRENT_PMI_EXTERNALS_LIBS_PATH)

    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PMI_EXTERNALS_LIBS_PATH}/cmake/modules")
endif()