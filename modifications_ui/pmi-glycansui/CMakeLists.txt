set(pmi_glycansui_app_SOURCES
    main.cpp
    GlycansUiMainWindow.cpp
)

set(pmi_glycansui_app_RESOURCES images.qrc)

qt5_add_resources(pmi_glycansui_app_SOURCES ${pmi_glycansui_app_RESOURCES})

add_executable(PMi-GlycansUI
    ${pmi_glycansui_app_SOURCES}
    app.rc
)

ecm_mark_nongui_executable(PMi-GlycansUI)

pmi_set_target_headers(PMi-GlycansUI
    PRIVATE
        GlycansUiMainWindow.h
)

target_link_libraries(PMi-GlycansUI
    PRIVATE
        pmi_modifications_ui
)

install(TARGETS PMi-GlycansUI ${INSTALL_TARGETS_DEFAULT_ARGS})

# for MSVS
source_group("Source Files" FILES ${pmi_glycansui_app_SOURCES})
set_property(TARGET PMi-GlycansUI PROPERTY FOLDER "Apps")
