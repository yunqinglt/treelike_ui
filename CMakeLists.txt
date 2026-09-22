cmake_minimum_required(VERSION 3.21)

project(treelike_ui VERSION 0.1.0 LANGUAGES C)

option(TREELIKE_UI_BUILD_TESTS
       "Build the platform-independent treelike UI tests"
       ${PROJECT_IS_TOP_LEVEL})
set(TREELIKE_UI_PIXEL_FORMAT "RGB565" CACHE STRING
    "Framebuffer format: RGB565 or RGB888")
set_property(CACHE TREELIKE_UI_PIXEL_FORMAT PROPERTY STRINGS RGB565 RGB888)

if(NOT TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB565" AND
   NOT TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB888")
    message(FATAL_ERROR
        "TREELIKE_UI_PIXEL_FORMAT must be RGB565 or RGB888")
endif()

function(treelike_ui_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(treelike_ui
    ui/ui_surface.c
    ui/ui_drawer.c
)
add_library(treelike_ui::treelike_ui ALIAS treelike_ui)
# Compatibility name for consumers of the original in-tree target.
add_library(ui_core ALIAS treelike_ui)

target_include_directories(treelike_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_compile_features(treelike_ui PUBLIC c_std_11)
treelike_ui_enable_warnings(treelike_ui)

if(TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB888")
    target_compile_definitions(treelike_ui PUBLIC PIXEL_FORMAT_RGB888)
else()
    target_compile_definitions(treelike_ui PUBLIC PIXEL_FORMAT_RGB565)
endif()

if(TREELIKE_UI_BUILD_TESTS)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()

    add_executable(treelike-ui-tests tests/ui_core_tests.c)
    target_link_libraries(treelike-ui-tests PRIVATE
                          treelike_ui::treelike_ui)
    target_compile_features(treelike-ui-tests PRIVATE c_std_11)
    treelike_ui_enable_warnings(treelike-ui-tests)
    add_test(NAME treelike-ui-tests COMMAND treelike-ui-tests)
endif()

message(STATUS "treelike_ui ${PROJECT_VERSION}")
message(STATUS "Pixel format: ${TREELIKE_UI_PIXEL_FORMAT}")
