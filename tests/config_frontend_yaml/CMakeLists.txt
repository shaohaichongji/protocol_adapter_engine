cmake_minimum_required(VERSION 3.25)
project(PaeYamlFrontendSlice LANGUAGES CXX)

if(NOT DEFINED PAE_SOURCE_DIR OR NOT IS_DIRECTORY "${PAE_SOURCE_DIR}")
  message(FATAL_ERROR "PAE_SOURCE_DIR must name the repository")
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
set(PAE_BUILD_YAML_FRONTEND ON CACHE BOOL "" FORCE)
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)
if(NOT TARGET pae_yaml_frontend)
  message(FATAL_ERROR "PAE YAML frontend target was not registered by the repository")
endif()

enable_testing()
add_executable(pae_yaml_frontend_tests frontend_tests.cpp)
target_compile_features(pae_yaml_frontend_tests PRIVATE cxx_std_17)
file(TO_CMAKE_PATH "${PAE_SOURCE_DIR}" pae_yaml_test_source_root)
target_compile_definitions(pae_yaml_frontend_tests PRIVATE PAE_TEST_SOURCE="${pae_yaml_test_source_root}")
target_link_libraries(pae_yaml_frontend_tests PRIVATE pae_yaml_frontend PAE::pae)
add_test(NAME pae_yaml_frontend_tests COMMAND pae_yaml_frontend_tests)

add_executable(pae_yaml_frontend_public_tests public_facade_tests.cpp)
target_compile_features(pae_yaml_frontend_public_tests PRIVATE cxx_std_17)
target_link_libraries(pae_yaml_frontend_public_tests PRIVATE PAE::yaml_frontend)
add_test(NAME pae_yaml_frontend_public_tests COMMAND pae_yaml_frontend_public_tests)

# These standalone consumers contain UTF-8 literals and include UTF-8 headers. Keep the
# setting local; split options are compatible with PAE::pae's source-charset propagation.
if(MSVC)
  target_compile_options(pae_yaml_frontend_tests PRIVATE /source-charset:utf-8 /execution-charset:utf-8)
  target_compile_options(pae_yaml_frontend_public_tests PRIVATE /source-charset:utf-8 /execution-charset:utf-8)
endif()
