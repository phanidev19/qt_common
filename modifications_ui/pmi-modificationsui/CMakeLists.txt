set(pmi_modificationsui_app_SOURCES
    main.cpp
    ModificationsUiMainWindow.cpp
)

set(pmi_modificationsui_app_RESOURCES images.qrc)

qt5_add_resources(pmi_modificationsui_app_SOURCES ${pmi_modificationsui_app_RESOURCES})

add_executable(PMi-ModificationsUI
    ${pmi_modificationsui_app_SOURCES}
    app.rc
)
ecm_mark_nongui_executable(PMi-ModificationsUI)

pmi_set_target_headers(PMi-ModificationsUI
    PRIVATE
        ModificationsUiMainWindow.h
)

target_link_libraries(PMi-ModificationsUI
    PRIVATE
        pmi_modifications_ui
)

install(TARGETS PMi-ModificationsUI ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
source_group("Source Files" FILES ${pmi_modificationsui_app_SOURCES})
set_property(TARGET PMi-ModificationsUI PROPERTY FOLDER "Apps")
