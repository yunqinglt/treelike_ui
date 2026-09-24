cmake_minimum_required(VERSION 3.21)

project(treelike_ui VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(_TREELIKE_UI_TESTS_DEFAULT OFF)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    set(_TREELIKE_UI_TESTS_DEFAULT ${BUILD_TESTING})
endif()

option(TREELIKE_UI_BUILD_TESTS
       "Build the platform-independent treelike UI tests"
       ${_TREELIKE_UI_TESTS_DEFAULT})
option(TREELIKE_UI_BUILD_SDL_PLAYER
       "Build the sdl_player development preview"
       OFF)
option(TREELIKE_UI_INSTALL
       "Generate install rules for treelike_ui"
       ${PROJECT_IS_TOP_LEVEL})
if(WIN32)
    set(_TREELIKE_UI_TRUETYPE_DEFAULT ON)
else()
    set(_TREELIKE_UI_TRUETYPE_DEFAULT OFF)
endif()
option(TREELIKE_UI_ENABLE_TRUETYPE
       "Enable the Win32 GDI TrueType/OpenType path backend"
       ${_TREELIKE_UI_TRUETYPE_DEFAULT})

if(TREELIKE_UI_ENABLE_TRUETYPE AND NOT WIN32)
    message(FATAL_ERROR
        "TREELIKE_UI_ENABLE_TRUETYPE currently requires a Win32 target")
endif()

set(_TREELIKE_UI_BUILD_TESTS_ENABLED ${TREELIKE_UI_BUILD_TESTS})
if(PROJECT_IS_TOP_LEVEL AND NOT BUILD_TESTING)
    set(_TREELIKE_UI_BUILD_TESTS_ENABLED OFF)
endif()

set(TREELIKE_UI_PIXEL_FORMAT "RGB565" CACHE STRING
    "Framebuffer format: RGB565 or RGB888")
set_property(CACHE TREELIKE_UI_PIXEL_FORMAT PROPERTY STRINGS RGB565 RGB888)

set(TREELIKE_UI_SDL_PLAYER_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl_player" CACHE PATH
    "Path to a local yunqinglt/sdl_player checkout")

if(NOT TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB565" AND
   NOT TREELIKE_UI_PIXEL_FORMAT STREQUAL "RGB888")
    message(FATAL_ERROR
        "TREELIKE_UI_PIXEL_FORMAT must be RGB565 or RGB888")
endif()

function(treelike_ui_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_subdirectory(ui)

if(_TREELIKE_UI_BUILD_TESTS_ENABLED)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()
    add_subdirectory(tests)
endif()

if(TREELIKE_UI_BUILD_SDL_PLAYER)
    get_filename_component(_TREELIKE_UI_SDL_PLAYER_SOURCE
        "${TREELIKE_UI_SDL_PLAYER_SOURCE_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT EXISTS "${_TREELIKE_UI_SDL_PLAYER_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR
            "sdl_player was not found at '${_TREELIKE_UI_SDL_PLAYER_SOURCE}'. "
            "Initialize it with 'git submodule update --init --recursive' or "
            "set TREELIKE_UI_SDL_PLAYER_SOURCE_DIR to another checkout.")
    endif()

    # sdl_player currently owns these cache entries when configured alone.
    # Synchronize them before adding it so every target uses one pixel_t ABI
    # and one test policy in this combined development build.
    set(SDL_PLAYER_PIXEL_FORMAT "${TREELIKE_UI_PIXEL_FORMAT}" CACHE STRING
        "Framebuffer format inherited from treelike_ui" FORCE)
    set(SDL_PLAYER_BUILD_TESTS "${_TREELIKE_UI_BUILD_TESTS_ENABLED}" CACHE BOOL
        "Build sdl_player tests with treelike_ui tests" FORCE)
    set(SDL_PLAYER_TREELIKE_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        CACHE PATH "treelike_ui source used by sdl_player" FORCE)
    if(TREELIKE_UI_ENABLE_TRUETYPE)
        set(_TREELIKE_UI_DEMO_TRUETYPE_FONT
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/fantasque_sans_mono/FantasqueSansMono-Regular.ttf")
        if(EXISTS "${_TREELIKE_UI_DEMO_TRUETYPE_FONT}" AND
           NOT SDL_PLAYER_TRUETYPE_FONT_PATH)
            set(SDL_PLAYER_TRUETYPE_FONT_PATH
                "${_TREELIKE_UI_DEMO_TRUETYPE_FONT}" CACHE FILEPATH
                "TrueType font copied into the sdl_player demo" FORCE)
        endif()
    endif()

    add_subdirectory("${_TREELIKE_UI_SDL_PLAYER_SOURCE}"
                     "${CMAKE_CURRENT_BINARY_DIR}/sdl_player")

    if(NOT TARGET sdl-player)
        message(FATAL_ERROR "sdl_player did not create the sdl-player target")
    endif()

    add_custom_target(treelike-ui-run
        COMMAND $<TARGET_FILE:sdl-player> --phase 3 --ui-tree-debug
        DEPENDS sdl-player
        WORKING_DIRECTORY $<TARGET_FILE_DIR:sdl-player>
        COMMENT "Running the treelike UI SDL preview (SPACE: next, ESC: quit)"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(treelike-ui-smoke
        COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy
                $<TARGET_FILE:sdl-player>
                --phase 3 --frames 10 --ui-tree-debug
        DEPENDS sdl-player
        WORKING_DIRECTORY $<TARGET_FILE_DIR:sdl-player>
        COMMENT "Running the headless treelike UI SDL smoke test"
        USES_TERMINAL
        VERBATIM
    )
endif()

if(TREELIKE_UI_INSTALL)
    set(_TREELIKE_UI_CMAKE_INSTALL_DIR
        "${CMAKE_INSTALL_LIBDIR}/cmake/treelike_ui")

    install(TARGETS treelike_ui
        EXPORT treelike_uiTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(FILES player_conf.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES
        ui/buffer_font_render.h
        ui/ui_types.h
        ui/ui_surface.h
        ui/ui_drawer.h
        ui/ui_object_raw.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ui"
    )

    configure_package_config_file(
        cmake/treelike_uiConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/treelike_uiConfig.cmake"
        INSTALL_DESTINATION "${_TREELIKE_UI_CMAKE_INSTALL_DIR}"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/treelike_uiConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT treelike_uiTargets
        FILE treelike_uiTargets.cmake
        NAMESPACE treelike_ui::
        DESTINATION "${_TREELIKE_UI_CMAKE_INSTALL_DIR}"
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/treelike_uiConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/treelike_uiConfigVersion.cmake"
        DESTINATION "${_TREELIKE_UI_CMAKE_INSTALL_DIR}"
    )
endif()

message(STATUS "treelike_ui ${PROJECT_VERSION}")
message(STATUS "Pixel format: ${TREELIKE_UI_PIXEL_FORMAT}")
message(STATUS "Core tests: ${_TREELIKE_UI_BUILD_TESTS_ENABLED}")
message(STATUS "SDL player: ${TREELIKE_UI_BUILD_SDL_PLAYER}")
message(STATUS "TrueType path backend: ${TREELIKE_UI_ENABLE_TRUETYPE}")
