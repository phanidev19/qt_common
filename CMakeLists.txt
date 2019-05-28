cmake_policy(SET CMP0048 NEW)
project(pmi_qt_common VERSION 2019.5.24) # Update this
cmake_minimum_required(VERSION 3.4.0 FATAL_ERROR)

set_property( GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_BINARY_DIR})

include(PMIDependencies)
include(GenerateExportHeader)
include(MacroLogFeature)
include(GNUInstallDirs)
include(PMICompilerSettings)
include(PMIMacros)

ensure_out_of_source_build()

# Fetch needed GIT information for the current build:
# - PMI_GIT_BRANCH_NAME is a branch or revision
include(GetGitRevisionDescription)
get_git_branch(PMI_GIT_BRANCH_NAME)
git_describe(PMI_GIT_VERSION --match "v[0-9].*" --tags --long)
# such tags are not present in this repository yet so disable this:
#git_describe(PMI_GIT_VERSION_INTACT --match "n[0-9].*" --tags --long)
#git_describe(PMI_GIT_VERSION_SUPERNOVO --match "s[0-9].*" --tags --long)

setup_cmake_install_prefix(${PMI_GIT_BRANCH_NAME})
setup_test_support()

# Default component name, it is for runtime files, the other is "Devel", interesting for developers
# only, e.g. contains headers). At the moment we're installing all components.
# https://cmake.org/cmake/help/v3.5/variable/CMAKE_INSTALL_DEFAULT_COMPONENT_NAME.html
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Package)

if(MSVC)
    if(PMI_USE_CONAN)
        find_package(PMIExternalsLibsDeprecated REQUIRED)
    else()
        find_package(PMIExternalsLibs REQUIRED)    
        include_directories(${PMIExternalsLibs_INCLUDE_DIR})
        install_pmi_externals_libs_files(
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            DEBUG_CONFIGURATIONS ${CMAKE_DEBUG_CONFIGURATIONS}
            RELEASE_CONFIGURATIONS ${CMAKE_RELEASE_CONFIGURATIONS}
            EXCLUDE_PATTERNS # Hack: remove when runtimes from PEL are no longer in use
                ^pmi_common_
                ^KProperty
                ^KF5Codecs
                ^data/icons
        )
        
        find_package(PMIQwt REQUIRED)
        find_package(PMISQLite REQUIRED)
        find_package(pmi_common_stable 2019.5.14 EXACT NO_MODULE REQUIRED)
        include(PMICopyCommonStablePWIZ)
    endif()
endif()

# Non-PMi dependencies
set(CMAKE_AUTOMOC ON) # convenience
set(CMAKE_AUTOUIC ON) # convenience
find_package(Qt5 ${PMI_REQUIRED_QT_VERSION} NO_MODULE
             REQUIRED Core Widgets Sql Xml XmlPatterns Network Svg Gui PrintSupport Qml Concurrent)

if(NOT PMI_USE_CONAN)
    set(OPENSSL_ROOT_DIR ${PMIExternalsLibs_ROOT_DIR})
    find_package(OpenSSL REQUIRED) #TODO remove openssl, it is not needed by QTC
    find_package(Eigen3 REQUIRED NO_MODULE)
endif()


if(WIN32)
    # Install Qt runtime dependencies
    include(PMIInstallQtRuntimeDependency)
    install_qt5_runtime_dependency(Qt5::QSQLiteDriverPlugin ${CMAKE_INSTALL_BINDIR}/sqldrivers)
    install_qt5_runtime_dependency(Qt5::QWindowsIntegrationPlugin ${CMAKE_INSTALL_BINDIR}/platforms)
endif()

# Global defs
if(APPLE)
    add_definitions(-DMAC_OS_X_VERSION_MAX_ALLOWED=MAC_OS_X_VERSION_10_8)
endif()

# Template for application manifests: QTC-specific
set(PMI_QTC_APP_MANIFEST_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/modules/PMIApp.exe.manifest.in")

# Install template for reuse in other projects that use MS Reader
install(FILES ${PMI_QTC_APP_MANIFEST_TEMPLATE} DESTINATION "lib/cmake/pmi_qt_common")

# Template for application configs: QTC-specific
set(PMI_QTC_APP_EXECONFIG_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/modules/PMIApp.exe.config.in")

# Install template for reuse in other projects that use MS Reader
install(FILES ${PMI_QTC_APP_EXECONFIG_TEMPLATE} DESTINATION "lib/cmake/pmi_qt_common")

# Global flags
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4150") # deletion of pointer to incomplete type 'type'; no destructor called
endif()

pmi_set_parallel_build(ON)

# Subdirs (have to be here, after options and testing code)
add_subdirectory(cache_file_creator)
add_subdirectory(common_core_mini)
add_subdirectory(common_tiles)
add_subdirectory(src)
add_subdirectory(common_widgets)
add_subdirectory(ms_reader_app)
add_subdirectory(ms_convert_command_line)
add_subdirectory(modifications_ui)
add_subdirectory(tests)

pmi_configure_file(pmi_qt_common)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
macro_display_feature_log()

if(PMI_USE_CONAN)
    pmi_setup_msvc_conan_path()
endif()
