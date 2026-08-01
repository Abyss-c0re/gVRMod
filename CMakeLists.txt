cmake_minimum_required(VERSION 3.10)
project(vrmod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Paths ──
set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/install/GarrysMod/garrysmod/lua/bin")

# ── Include dirs (used by both targets) ──
set(VRMOD_INCLUDE_DIRS
    ${DEPS_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# ── Sources by module ──
set(CORE_SOURCES
    src/core/vrmod_log.cpp
)

set(INPUT_SOURCES
    src/input/xr_input.cpp
)

set(RENDER_XR_SESSION
    src/rendering/openxr/xr_session.cpp
)

set(LUA_SOURCES
    src/lua/lua_interface.cpp
)

if(WIN32)
    set(RENDER_PLATFORM_SOURCES
        src/rendering/d3d/d3d_hooks.cpp
        src/rendering/openxr/xr_render_d3d.cpp
    )
    set(VRMOD_OUTPUT_NAME "gmcl_vrmod_win64")
    set(VRMOD_PLATFORM_LIBS d3d11 dxgi)
else()
    set(RENDER_PLATFORM_SOURCES
        src/rendering/opengl/gl_hooks.cpp
        src/rendering/openxr/xr_render.cpp
    )
    set(VRMOD_OUTPUT_NAME "gmcl_vrmod_linux64")
    set(VRMOD_PLATFORM_LIBS dl X11 GL GLX)
endif()

# ==============================================================
#  RELEASE target: shared library (the gmod module) — OpenXR
# ==============================================================
option(VRMOD_BUILD_TESTS "Build the dev test runner" OFF)

add_library(vrmod_release SHARED
    ${CORE_SOURCES}
    ${INPUT_SOURCES}
    ${RENDER_PLATFORM_SOURCES}
    ${RENDER_XR_SESSION}
    ${LUA_SOURCES}
)

target_include_directories(vrmod_release PRIVATE ${VRMOD_INCLUDE_DIRS})

if(WIN32)
    target_compile_definitions(vrmod_release PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(vrmod_release PRIVATE /O2 /W3 /wd4996 /MT)
    endif()
else()
    target_compile_options(vrmod_release PRIVATE -O3 -m64)
endif()

# OpenXR loader is obtained at runtime (dlopen / LoadLibrary).
target_link_libraries(vrmod_release PRIVATE ${VRMOD_PLATFORM_LIBS})

set_target_properties(vrmod_release PROPERTIES
    OUTPUT_NAME "${VRMOD_OUTPUT_NAME}"
    PREFIX ""
    SUFFIX ".dll"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

if(NOT WIN32)
    set_target_properties(vrmod_release PROPERTIES
        BUILD_RPATH "\$ORIGIN"
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# ==============================================================
#  DEV target: test runner executable (no game, no GL, no real VR)
# ==============================================================
if(VRMOD_BUILD_TESTS)
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_input.cpp
        tests/test_lua.cpp
        tests/test_distortion.cpp
    )

    add_executable(vrmod_tests
        ${CORE_SOURCES}
        ${TEST_SOURCES}
    )

    target_include_directories(vrmod_tests PRIVATE
        ${VRMOD_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/tests
    )

    target_compile_definitions(vrmod_tests PRIVATE
        VRMOD_DEV_BUILD
        VRMOD_TESTING
    )

    if(WIN32)
        target_compile_definitions(vrmod_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(vrmod_tests PRIVATE -g -O0 -m64)
        target_link_libraries(vrmod_tests PRIVATE dl)
    endif()

    set_target_properties(vrmod_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
