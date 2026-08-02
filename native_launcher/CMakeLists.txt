cmake_minimum_required(VERSION 3.14)
project(cube_webui_launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cube_webui_launcher
  src/main.cpp
  src/maps_scan.cpp
  src/addons_mgr.cpp
  src/gmod_spawn.cpp
  src/ui_panel.cpp
  src/xr_app.cpp
)

target_include_directories(cube_webui_launcher PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
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

install(TARGETS cube_webui_launcher RUNTIME DESTINATION bin)
