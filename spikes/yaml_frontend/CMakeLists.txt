cmake_minimum_required(VERSION 3.20)
project(pae_yaml_frontend_probe LANGUAGES C CXX)

if(NOT DEFINED LIBYAML_SOURCE_DIR OR NOT EXISTS "${LIBYAML_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set LIBYAML_SOURCE_DIR to the isolated libyaml 0.2.5 source")
endif()

set(BUILD_TESTING OFF CACHE BOOL "Keep the independent probe small" FORCE)
add_subdirectory("${LIBYAML_SOURCE_DIR}" libyaml-build)
add_executable(pae_yaml_frontend_probe probe.cpp)
target_compile_features(pae_yaml_frontend_probe PRIVATE cxx_std_17)
target_link_libraries(pae_yaml_frontend_probe PRIVATE yaml)
