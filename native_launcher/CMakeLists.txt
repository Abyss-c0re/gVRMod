cmake_minimum_required(VERSION 3.14)
project(cube_webui_launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Modular layout — see src/README.md
# Shared OpenXR with vrmod: ../shared/openxr
set(CUBE_SHARED_OPENXR
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/openxr/openxr_bindings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/openxr/openxr_shell_input.cpp
)

set(CUBE_WEBUI_SOURCES
  src/main.cpp
  src/paths.cpp
  src/panel_config.cpp
  src/world_panel.cpp
  src/glx_context.cpp
  src/gl_render.cpp
  src/xr_input.cpp
  src/xr_util.cpp
  src/host_cmd.cpp
  src/xr_app.cpp
  src/maps_scan.cpp
  src/addons_mgr.cpp
  src/bindings_mgr.cpp
  src/gmod_spawn.cpp
  src/ui_panel.cpp
  src/smx_player.cpp
  src/ambient_backend.cpp
  ${CUBE_SHARED_OPENXR}
)

add_executable(cube_webui_launcher ${CUBE_WEBUI_SOURCES})

target_include_directories(cube_webui_launcher PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/openxr
  /usr/include
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
find_library(OPENXR_LIB openxr_loader REQUIRED)
find_library(GL_LIB GL REQUIRED)
find_library(GLX_LIB GLX REQUIRED)

target_link_libraries(cube_webui_launcher PRIVATE
  ${OPENXR_LIB}
  ${GL_LIB}
  ${GLX_LIB}
  ${X11_LIBRARIES}
  dl
  m
  pthread
)

target_compile_options(cube_webui_launcher PRIVATE -O2 -Wall)

set_target_properties(cube_webui_launcher PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../install/native"
)

add_custom_command(TARGET cube_webui_launcher POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/cube_webui.conf"
    "${CMAKE_SOURCE_DIR}/../install/native/cube_webui.conf"
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_SOURCE_DIR}/../install/native/assets/ambient"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient/cube_hold.ogg"
    "${CMAKE_SOURCE_DIR}/../install/native/assets/ambient/cube_hold.ogg"
  COMMENT "Install cube_webui.conf + ambient assets next to binary"
)

install(TARGETS cube_webui_launcher RUNTIME DESTINATION bin)
install(FILES cube_webui.conf cube_webui.conf.example DESTINATION share/gvrmod)

# ── Offline unit tests (no OpenXR runtime / no display required for math3d) ──
option(CUBE_BUILD_TESTS "Build cube launcher unit tests" ON)
if(CUBE_BUILD_TESTS)
  add_executable(cube_launcher_tests
    tests/test_main.cpp
  )
  target_include_directories(cube_launcher_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/openxr
  )
  target_compile_options(cube_launcher_tests PRIVATE -g -O0 -Wall)
  set_target_properties(cube_launcher_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()
