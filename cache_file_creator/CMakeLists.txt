set(cache_file_creator_SOURCES
    main.cpp
)

pmi_add_executable(PMi-CacheFileCreator ${cache_file_creator_SOURCES})
ecm_mark_nongui_executable(PMi-CacheFileCreator)

target_link_libraries(PMi-CacheFileCreator
    pmi_common_ms
    Qt5::Core
)

pmi_add_manifest(PMi-CacheFileCreator ${PMI_QTC_APP_MANIFEST_TEMPLATE})
pmi_add_execonfig(PMi-CacheFileCreator ${PMI_QTC_APP_EXECONFIG_TEMPLATE})

install(TARGETS PMi-CacheFileCreator ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
source_group("Source Files" FILES ${cache_file_creator_SOURCES})
set_property(TARGET PMi-CacheFileCreator PROPERTY FOLDER "Apps")
