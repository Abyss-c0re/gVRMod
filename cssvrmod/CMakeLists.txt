cmake_minimum_required(VERSION 3.14)
project(cssvrmod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CSSVR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(GVRMOD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CSSVR_INSTALL "${GVRMOD_ROOT}/install/cssvrmod")

# ── Pure laws + catalog + launch (no GL / no XR) ──
add_library(cssvrmod_core STATIC
  src/weapons.cpp
  src/launch.cpp
  src/source_if.cpp
)
target_include_directories(cssvrmod_core PUBLIC
  ${CSSVR_ROOT}/include
)
target_link_libraries(cssvrmod_core PUBLIC dl)

# ── Offline tests ──
add_executable(cssvrmod_tests
  tests/test_main.cpp
  tests/test_hand_bullet.cpp
  tests/test_melee.cpp
  tests/test_collision.cpp
  tests/test_aim.cpp
  tests/test_weapons.cpp
  tests/test_input.cpp
  tests/test_launch.cpp
  tests/test_tick.cpp
)
target_include_directories(cssvrmod_tests PRIVATE
  ${CSSVR_ROOT}/include
  ${CSSVR_ROOT}/tests
)
target_link_libraries(cssvrmod_tests PRIVATE cssvrmod_core)
set_target_properties(cssvrmod_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CSSVR_ROOT}/build"
)

# ── Launcher ──
add_executable(CSSVR src/app_main.cpp)
target_include_directories(CSSVR PRIVATE ${CSSVR_ROOT}/include)
target_link_libraries(CSSVR PRIVATE cssvrmod_core)
set_target_properties(CSSVR PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CSSVR_INSTALL}"
)
add_custom_command(TARGET CSSVR POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CSSVR_INSTALL}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CSSVR_ROOT}/cfg/cssvrmod.cfg" "${CSSVR_INSTALL}/cssvrmod.cfg"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CSSVR_ROOT}/scripts/CSSVR.sh" "${CSSVR_INSTALL}/CSSVR.sh"
  COMMENT "Install cssvrmod.cfg + CSSVR.sh next to launcher"
)

# ── LD_PRELOAD hook (GL + OpenXR). Optional if loader missing. ──
option(CSSVR_BUILD_HOOK "Build libcssvrmod_hook.so" ON)
if(CSSVR_BUILD_HOOK AND NOT WIN32)
  find_library(OPENXR_LIB openxr_loader)
  find_library(GL_LIB GL)
  find_library(GLX_LIB GLX)
  find_package(X11)
  if(OPENXR_LIB AND GL_LIB AND GLX_LIB AND X11_FOUND)
    add_library(cssvrmod_hook SHARED
      src/hook_ctor.cpp
      src/hook_gl.cpp
      src/hook_window.cpp
      src/hook_d3d9.cpp
      src/hook_vk.cpp
      src/togl_adapter.cpp
      src/xr_host.cpp
    )
    target_include_directories(cssvrmod_hook PRIVATE
      ${CSSVR_ROOT}/include
      ${CSSVR_ROOT}/src
      ${GVRMOD_ROOT}/shared/openxr
      ${GVRMOD_ROOT}/deps
      ${GVRMOD_ROOT}/deps/openxr
    )
    target_link_libraries(cssvrmod_hook PRIVATE
      cssvrmod_core
      ${OPENXR_LIB}
      ${GL_LIB}
      ${GLX_LIB}
      ${X11_LIBRARIES}
      dl
    )
    target_compile_options(cssvrmod_hook PRIVATE -O2 -Wall -fvisibility=default)
    set_target_properties(cssvrmod_hook PROPERTIES
      OUTPUT_NAME "cssvrmod_hook"
      PREFIX "lib"
      LIBRARY_OUTPUT_DIRECTORY "${CSSVR_INSTALL}"
      BUILD_RPATH "\$ORIGIN"
      INSTALL_RPATH "\$ORIGIN"
    )
  else()
    message(WARNING "cssvrmod: hook skipped (need OpenXR + GL + GLX + X11)")
  endif()
endif()
