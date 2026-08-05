cmake_minimum_required(VERSION 3.14)
project(smx_proto LANGUAGES CXX)

# Standalone SMX lab binary. Never required by CubeUI / gmcl_vrmod_*.
#   cmake -S experimental/smx_proto -B experimental/smx_proto/build
#   cmake --build experimental/smx_proto/build -j
#   GVRMOD_SMX_BIND=0.0.0.0:17740 ./experimental/smx_proto/build/smx_proto

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(smx_proto
  smx_proto_main.cpp
  smx_player.cpp
)

target_include_directories(smx_proto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
