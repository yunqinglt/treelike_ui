add_executable(treelike-ui-tests ui_core_tests.c)
if(TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB565")
    target_sources(treelike-ui-tests PRIVATE
        fixtures/54A99503218B6635058DAE67B9FF007B_resized.c
        fixtures/54A99503218B6635058DAE67B9FF007B_resized.h
    )
endif()
target_link_libraries(treelike-ui-tests PRIVATE treelike_ui::treelike_ui)
target_compile_definitions(treelike-ui-tests PRIVATE
    "TREELIKE_UI_TEST_BITMAP_FONT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/path_bitmap.tlf\""
    "TREELIKE_UI_TEST_VECTOR_FONT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/path_vector.tlf\""
)
if(TREELIKE_UI_ENABLE_TRUETYPE)
    set(_TREELIKE_UI_TEST_TRUETYPE_FONT
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts/fantasque_sans_mono/FantasqueSansMono-Regular.ttf")
    if(EXISTS "${_TREELIKE_UI_TEST_TRUETYPE_FONT}")
        file(TO_CMAKE_PATH "${_TREELIKE_UI_TEST_TRUETYPE_FONT}"
             _TREELIKE_UI_TEST_TRUETYPE_FONT)
        target_compile_definitions(treelike-ui-tests PRIVATE
            "TREELIKE_UI_TEST_TRUETYPE_FONT_PATH=\"${_TREELIKE_UI_TEST_TRUETYPE_FONT}\"")
    endif()
endif()
target_compile_features(treelike-ui-tests PRIVATE c_std_11)
set_target_properties(treelike-ui-tests PROPERTIES
    C_EXTENSIONS OFF
)
treelike_ui_enable_warnings(treelike-ui-tests)

add_test(NAME treelike-ui-tests COMMAND treelike-ui-tests)
