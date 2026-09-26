get_filename_component(
  _pae_ryml_header
  "${CMAKE_CURRENT_LIST_DIR}/../../third_party/rapidyaml/rapidyaml.hpp"
  ABSOLUTE
)
if(NOT EXISTS "${_pae_ryml_header}")
  message(FATAL_ERROR "PAE YAML frontend requires third_party/rapidyaml/rapidyaml.hpp")
endif()
file(SHA256 "${_pae_ryml_header}" _ryml_sha)
if(NOT _ryml_sha STREQUAL "0d0b8076174cf62f034406b03529fda542ebc9a17506d3bd6d949aedc4bfa6ab")
  message(FATAL_ERROR "PAE YAML frontend rapidyaml 0.16.0 header SHA256 mismatch")
endif()

add_library(pae_yaml_frontend STATIC frontend.cpp)
target_compile_features(pae_yaml_frontend PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(pae_yaml_frontend PRIVATE /utf-8)
endif()
target_include_directories(pae_yaml_frontend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(_ryml_dir "${_pae_ryml_header}" DIRECTORY)
target_include_directories(pae_yaml_frontend PRIVATE "${_ryml_dir}")
