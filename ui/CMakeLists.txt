add_library(treelike_ui STATIC
    buffer_font_render.c
    ui_surface.c
    ui_drawer.c
    ui_object_raw.c
    buffer_font_render.h
    ui_types.h
    ui_surface.h
    ui_drawer.h
    ui_object_raw.h
    ../player_conf.h
)

if(TREELIKE_UI_ENABLE_TRUETYPE)
    target_sources(treelike_ui PRIVATE
        buffer_font_win32.c
        buffer_font_win32.h
    )
endif()
add_library(treelike_ui::treelike_ui ALIAS treelike_ui)

# Compatibility name for consumers of the original in-tree target.
add_library(ui_core ALIAS treelike_ui)

target_include_directories(treelike_ui PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(treelike_ui PUBLIC c_std_11)
target_compile_definitions(treelike_ui PUBLIC
    TREELIKE_UI_HAS_FONT_RENDER=1
    TREELIKE_UI_HAS_OBJECT_RAW=1
)
if(TREELIKE_UI_ENABLE_TRUETYPE)
    target_compile_definitions(treelike_ui PUBLIC
        TREELIKE_UI_HAS_TRUETYPE=1
    )
    target_link_libraries(treelike_ui PRIVATE Gdi32)
endif()
set_target_properties(treelike_ui PROPERTIES
    C_EXTENSIONS OFF
)
treelike_ui_enable_warnings(treelike_ui)

if(TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB888")
    target_compile_definitions(treelike_ui PUBLIC PIXEL_FORMAT_RGB888)
else()
    target_compile_definitions(treelike_ui PUBLIC PIXEL_FORMAT_RGB565)
endif()
