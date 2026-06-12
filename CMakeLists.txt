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

set(RENDER_GL_SOURCES
    src/rendering/opengl/gl_hooks.cpp
)

set(RENDER_XR_SOURCES
    src/rendering/openxr/xr_session.cpp
    src/rendering/openxr/xr_render.cpp
)

set(LUA_SOURCES
    src/lua/lua_interface.cpp
)

# ==============================================================
#  RELEASE target: shared library (the gmod module) — OpenXR
# ==============================================================
option(VRMOD_BUILD_TESTS "Build the dev test runner" OFF)

add_library(vrmod_release SHARED
    ${CORE_SOURCES}
    ${INPUT_SOURCES}
    ${RENDER_GL_SOURCES}
    ${RENDER_XR_SOURCES}
    ${LUA_SOURCES}
)

target_include_directories(vrmod_release PRIVATE ${VRMOD_INCLUDE_DIRS})
target_compile_options(vrmod_release PRIVATE -O3 -m64)
# OpenXR loader is obtained at runtime via dlopen (see XR_LoadLoader in xr_session.cpp).
# We do NOT link against libopenxr_loader here. This allows the module to load on
# systems where only an OpenXR *runtime* (Monado, ALVR layer, etc.) is present via
# active_runtime.json, without the loader development package being in the library
# search path. X11 + GL + GLX are real dependencies (direct GLX calls for graphics binding).
target_link_libraries(vrmod_release PRIVATE dl X11 GL GLX)

set_target_properties(vrmod_release PROPERTIES
    OUTPUT_NAME "gmcl_vrmod_linux64"
    PREFIX ""
    SUFFIX ".dll"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)

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

    # Only compile modules that are testable without real HW
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

    target_compile_options(vrmod_tests PRIVATE -g -O0 -m64)
    target_link_libraries(vrmod_tests PRIVATE dl)

    set_target_properties(vrmod_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
