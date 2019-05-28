set(pmi_common_tiles_LIB_SOURCES
    src/CheckerDataProvider.cpp
    src/ImagePatternTileIterator.cpp
    src/ImageTileIterator.cpp
    src/MzScanIndexNonUniformTileRectIterator.cpp
    src/NonUniformTile.cpp
    src/NonUniformTileDevice.cpp
    src/NonUniformTileIntensityIndex.cpp
    src/NonUniformTileManager.cpp
    src/NonUniformTileMaxIntensityFinder.cpp
    src/NonUniformTilePartIterator.cpp
    src/NonUniformTilePoint.cpp
    src/NonUniformTileRange.cpp
    src/NonUniformTileStore.cpp
    src/NonUniformTileStoreMemory.cpp
    src/NonUniformTileStoreSqlite.cpp
    src/pmi_common_tiles_debug.cpp
    src/RandomBilinearTileIterator.cpp
    src/RandomNonUniformTileIterator.cpp
    src/RandomTileIterator.cpp
    src/SequentialNonUniformTileIterator.cpp
    src/SequentialTileIterator.cpp
    src/Tile.cpp
    src/TileDataProvider.cpp
    src/TileLevelSelector.cpp
    src/TileManager.cpp
    src/TilePositionConverter.cpp
    src/TileRange.cpp
    src/TileRaster.cpp
    src/TileStore.cpp
    src/TileStoreMemory.cpp
    src/TileStoreSqlite.cpp
    src/db/ByologicAnnotationDao.cpp
    src/db/ByspecAnnotationDao.cpp
    src/db/NonUniformTilesDao.cpp
    src/db/NonUniformTilesInfoDao.cpp
    src/db/NonUniformTilesMetaInfoDao.cpp
    src/db/NonUniformTilesPartDao.cpp
    src/db/NonUniformTilesSerialization.cpp     
)

set(pmi_common_tiles_LIB_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

add_library(pmi_common_tiles SHARED ${pmi_common_tiles_LIB_SOURCES})

target_link_libraries(pmi_common_tiles
    PUBLIC
        pmi_common_core_mini
)

set_property(TARGET pmi_common_tiles PROPERTY FOLDER "Libs")

generate_export_header(pmi_common_tiles)

pmi_set_target_headers(pmi_common_tiles
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_tiles_export.h
        src/db/ByologicAnnotationDao.h
        src/db/ByspecAnnotationDao.h
        src/db/NonUniformTilesDao.h
        src/db/NonUniformTilesInfoDao.h
        src/db/NonUniformTilesMetaInfoDao.h
        src/db/NonUniformTilesPartDao.h
        src/db/NonUniformTilesSerialization.h
        src/CheckerDataProvider.h
        src/ImagePatternTileIterator.h
        src/ImageTileIterator.h
        src/MzScanIndexRect.h
        src/MzScanIndexNonUniformTileRectIterator.h
        src/NonUniformTilePoint.h
        src/NonUniformTile.h
        src/NonUniformTileDevice.h
        src/NonUniformTileBase.h
        src/NonUniformTileIntensityIndex.h
        src/NonUniformTileManager.h
        src/NonUniformTileMaxIntensityFinder.h
        src/NonUniformTilePartIterator.h
        src/NonUniformTileRange.h
        src/NonUniformTileStore.h
        src/NonUniformTileStoreBase.h
        src/NonUniformTileStoreMemory.h
        src/NonUniformTileStoreSqlite.h
        src/pmi_common_tiles_debug.h
        src/RandomBilinearTileIterator.h
        src/RandomNonUniformTileIterator.h
        src/RandomTileIterator.h
        src/ScanIndexInterval.h
        src/SequentialNonUniformTileIterator.h
        src/SequentialTileIterator.h
        src/Tile.h
        src/TileDataProvider.h
        src/TileLevelSelector.h
        src/TileManager.h
        src/TilePositionConverter.h
        src/TileRange.h
        src/TileRaster.h
        src/TileStore.h
        src/TileStoreMemory.h
        src/TileStoreSqlite.h
    INSTALL_DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_tiles
)

target_include_directories(pmi_common_tiles
    PUBLIC "$<BUILD_INTERFACE:${pmi_common_tiles_LIB_INCLUDE_DIRS}>"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pmi_common_tiles>"
)

# Create a Config.cmake and a ConfigVersion.cmake file and install them
include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_PREFIX "lib/cmake/pmi_common_tiles")
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}")
configure_package_config_file(pmi_common_tilesConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_tilesConfig.cmake
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pmi_common_tilesConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_tilesConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pmi_common_tilesConfigVersion.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    COMPONENT Devel
)
install(TARGETS pmi_common_tiles
        EXPORT pmi_common_tilesTargets
        ${INSTALL_TARGETS_DEFAULT_ARGS}
)
pmi_install_pdb(pmi_common_tiles)
install(EXPORT pmi_common_tilesTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE pmi_common_tilesTargets.cmake
        COMPONENT Devel
)

add_subdirectory(tests)

# for MSVS
source_group("Source Files" FILES ${pmi_common_tiles_LIB_SOURCES})
