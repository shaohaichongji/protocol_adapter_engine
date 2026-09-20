cmake_minimum_required(VERSION 3.25)
project(PaeProtocolLabStandalone LANGUAGES C CXX)

option(PAE_LAB_BUILD_TESTING "Build the installed-SDK Lab validation tests" ON)
set(PAE_LAB_EXPECTED_LIBRARY_KIND "STATIC" CACHE STRING "Required installed PAE package kind")
set_property(CACHE PAE_LAB_EXPECTED_LIBRARY_KIND PROPERTY STRINGS STATIC SHARED)
set(PAE_LAB_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Lab whitelist source root")
set(PAE_SDK_ROOT "" CACHE PATH "Installed PAE SDK root")
set(PAE_QT_ROOT "" CACHE PATH "Fixed Qt 5.13 root")
set(PAE_LAB_DEPENDENCY_ROOT "" CACHE PATH "Locked Lab dependency root")
set(PAE_LAB_CONFIG_ROOT "${PAE_LAB_SOURCE_ROOT}/standalone-configs" CACHE PATH "Lab config whitelist root")
set(PAE_LAB_DEPLOY_ROOT "${CMAKE_BINARY_DIR}/deploy" CACHE PATH "Bounded Lab deployment root")

foreach(required IN ITEMS PAE_LAB_SOURCE_ROOT PAE_SDK_ROOT PAE_QT_ROOT PAE_LAB_DEPENDENCY_ROOT PAE_LAB_CONFIG_ROOT)
  if(NOT IS_ABSOLUTE "${${required}}")
    message(FATAL_ERROR "${required} must be an absolute path")
  endif()
  cmake_path(NORMAL_PATH ${required} OUTPUT_VARIABLE normalized_${required})
  set(${required} "${normalized_${required}}")
endforeach()

if(NOT EXISTS "${PAE_SDK_ROOT}/PROVENANCE.json" OR
   NOT IS_DIRECTORY "${PAE_SDK_ROOT}/include/pae" OR
   NOT IS_DIRECTORY "${PAE_SDK_ROOT}/lib")
  message(FATAL_ERROR "PAE_SDK_ROOT must name one installed SDK package root exactly")
endif()
find_package(PAE CONFIG REQUIRED PATHS "${PAE_SDK_ROOT}" NO_DEFAULT_PATH)
cmake_path(NORMAL_PATH PAE_DIR OUTPUT_VARIABLE normalized_pae_dir)
cmake_path(IS_PREFIX PAE_SDK_ROOT "${normalized_pae_dir}" NORMALIZE pae_dir_is_sdk)
if(NOT pae_dir_is_sdk)
  message(FATAL_ERROR "PAE_DIR escaped PAE_SDK_ROOT: ${normalized_pae_dir}")
endif()
if(NOT PAE_LAB_EXPECTED_LIBRARY_KIND STREQUAL "STATIC" AND
   NOT PAE_LAB_EXPECTED_LIBRARY_KIND STREQUAL "SHARED")
  message(FATAL_ERROR "PAE_LAB_EXPECTED_LIBRARY_KIND must be STATIC or SHARED")
endif()
if(NOT PAE_LIBRARY_KIND STREQUAL PAE_LAB_EXPECTED_LIBRARY_KIND)
  message(FATAL_ERROR
    "Installed PAE package kind mismatch: expected ${PAE_LAB_EXPECTED_LIBRARY_KIND}, got ${PAE_LIBRARY_KIND}")
endif()
if(NOT PAE_PACKAGE_CONFIGURATION STREQUAL "Debug" AND
   NOT PAE_PACKAGE_CONFIGURATION STREQUAL "Release")
  message(FATAL_ERROR "Installed PAE package has unsupported configuration: ${PAE_PACKAGE_CONFIGURATION}")
endif()

string(TOUPPER "${PAE_PACKAGE_CONFIGURATION}" pae_package_configuration_upper)
if(PAE_LIBRARY_KIND STREQUAL "SHARED")
  get_target_property(pae_import_library PAE::pae "IMPORTED_IMPLIB_${pae_package_configuration_upper}")
  get_target_property(pae_runtime_dll PAE::pae "IMPORTED_LOCATION_${pae_package_configuration_upper}")
  foreach(input IN ITEMS pae_import_library pae_runtime_dll)
    if(NOT ${input} OR NOT EXISTS "${${input}}")
      message(FATAL_ERROR "PAE::pae lacks an existing shared ${input} for ${PAE_PACKAGE_CONFIGURATION}")
    endif()
    cmake_path(NORMAL_PATH ${input} OUTPUT_VARIABLE normalized_${input})
    cmake_path(IS_PREFIX PAE_SDK_ROOT "${normalized_${input}}" NORMALIZE ${input}_in_sdk)
    if(NOT ${input}_in_sdk)
      message(FATAL_ERROR "PAE::pae ${input} escaped PAE_SDK_ROOT: ${normalized_${input}}")
    endif()
  endforeach()
  file(SHA256 "${normalized_pae_runtime_dll}" pae_runtime_sha256)
  file(STRINGS "${PAE_SDK_ROOT}/SHA256SUMS.txt" pae_runtime_manifest_line
    REGEX "^[0-9A-Fa-f]+  bin/pae\\.dll$")
  if(NOT pae_runtime_manifest_line MATCHES "^([0-9A-Fa-f]+)  bin/pae\\.dll$")
    message(FATAL_ERROR "Installed shared PAE package lacks a unique bin/pae.dll manifest entry")
  endif()
  string(TOLOWER "${CMAKE_MATCH_1}" pae_runtime_manifest_sha256)
  string(TOLOWER "${pae_runtime_sha256}" pae_runtime_sha256_lower)
  if(NOT pae_runtime_sha256_lower STREQUAL pae_runtime_manifest_sha256)
    message(FATAL_ERROR "Installed shared PAE runtime hash does not match SHA256SUMS.txt")
  endif()
  cmake_path(GET normalized_pae_runtime_dll PARENT_PATH pae_runtime_directory)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test-bin")
  set(pae_test_runtime_directory
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PAE_PACKAGE_CONFIGURATION}")
  file(MAKE_DIRECTORY "${pae_test_runtime_directory}")
  file(COPY_FILE "${normalized_pae_runtime_dll}"
    "${pae_test_runtime_directory}/pae.dll" ONLY_IF_DIFFERENT)
  file(SHA256 "${pae_test_runtime_directory}/pae.dll" pae_test_runtime_sha256)
  string(TOLOWER "${pae_test_runtime_sha256}" pae_test_runtime_sha256)
  if(NOT pae_test_runtime_sha256 STREQUAL pae_runtime_sha256_lower)
    message(FATAL_ERROR "Shared PAE test runtime copy hash mismatch")
  endif()
else()
  get_target_property(pae_static_library PAE::pae "IMPORTED_LOCATION_${pae_package_configuration_upper}")
  if(NOT pae_static_library OR NOT EXISTS "${pae_static_library}")
    message(FATAL_ERROR "PAE::pae lacks an existing static library for ${PAE_PACKAGE_CONFIGURATION}")
  endif()
  cmake_path(NORMAL_PATH pae_static_library OUTPUT_VARIABLE normalized_pae_static_library)
  cmake_path(IS_PREFIX PAE_SDK_ROOT "${normalized_pae_static_library}" NORMALIZE pae_static_library_in_sdk)
  if(NOT pae_static_library_in_sdk)
    message(FATAL_ERROR "PAE::pae static library escaped PAE_SDK_ROOT: ${normalized_pae_static_library}")
  endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/PaeLabQt513.cmake")
pae_lab_require_toolchain()
pae_lab_import_qt()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(PAE_LAB_BUILD_TESTING)
  enable_testing()
endif()

add_library(pae_lab_project_options INTERFACE)
add_library(pae::project_options ALIAS pae_lab_project_options)
if(MSVC)
  target_compile_options(pae_lab_project_options INTERFACE /W4 /permissive- /Zc:__cplusplus /utf-8)
  target_compile_definitions(pae_lab_project_options INTERFACE _CRT_SECURE_NO_WARNINGS)
endif()

set(yyjson_header "${PAE_LAB_DEPENDENCY_ROOT}/src/yyjson.h")
set(yyjson_source "${PAE_LAB_DEPENDENCY_ROOT}/src/yyjson.c")
set(yyjson_license "${PAE_LAB_DEPENDENCY_ROOT}/LICENSE")
set(yyjson_lock "${PAE_LAB_DEPENDENCY_ROOT}/dependency.lock.json")
foreach(input IN ITEMS yyjson_header yyjson_source yyjson_license yyjson_lock)
  if(NOT EXISTS "${${input}}")
    message(FATAL_ERROR "Missing locked yyjson input: ${${input}}")
  endif()
endforeach()
file(SHA256 "${yyjson_header}" yyjson_header_sha)
file(SHA256 "${yyjson_source}" yyjson_source_sha)
file(SHA256 "${yyjson_license}" yyjson_license_sha)
if(NOT yyjson_header_sha STREQUAL "175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3" OR
   NOT yyjson_source_sha STREQUAL "ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a" OR
   NOT yyjson_license_sha STREQUAL "45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947")
  message(FATAL_ERROR "Locked yyjson source or license hash mismatch")
endif()
file(READ "${yyjson_lock}" yyjson_lock_text)
if(NOT yyjson_lock_text MATCHES "\"version\"[ \t]*:[ \t]*\"0\\.12\\.0\"" OR
   NOT yyjson_lock_text MATCHES "8b4a38dc994a110abaec8a400615567bd996105f")
  message(FATAL_ERROR "yyjson lock does not identify approved 0.12.0 input")
endif()
add_library(pae_lab_yyjson_vendor STATIC "${yyjson_source}")
target_include_directories(pae_lab_yyjson_vendor PUBLIC "${PAE_LAB_DEPENDENCY_ROOT}/src")

set(ui_dir "${PAE_LAB_SOURCE_ROOT}/tools/protocol_lab_ui")
set(ascii_dir "${PAE_LAB_SOURCE_ROOT}/tools/protocol_lab_ascii")
set(binary_dir "${PAE_LAB_SOURCE_ROOT}/tools/protocol_lab_binary")

add_library(pae_lab_features INTERFACE)
target_compile_definitions(pae_lab_features INTERFACE
  PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY=1
  PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE=1
  PAE_BUILD_PROTOCOL_LAB_BINARY_UI=1
  PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2=1
  PAE_ENABLE_SCHEMA_V05_COMPILER=1
  PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=1
  PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=1
  PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1=1
  PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=1
  PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM=1
  PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI=1
  PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=1
  PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER=1)

add_library(pae_protocol_lab_binary_public_h1 STATIC
  "${binary_dir}/public_binary_decode.cpp" "${binary_dir}/public_binary_decode.h")
target_include_directories(pae_protocol_lab_binary_public_h1 PUBLIC "${binary_dir}")
target_link_libraries(pae_protocol_lab_binary_public_h1 PUBLIC PAE::pae pae_lab_features PRIVATE pae::project_options)

set(pae_lab_ascii_public_sources
  "${ascii_dir}/public_ascii_offline_adapter.cpp" "${ascii_dir}/public_ascii_offline_adapter.h"
  "${ascii_dir}/public_ascii_host_adapter.cpp" "${ascii_dir}/public_ascii_host_adapter.h")
add_library(pae_protocol_lab_ascii_public_a1 STATIC ${pae_lab_ascii_public_sources})
target_include_directories(pae_protocol_lab_ascii_public_a1 PUBLIC "${ascii_dir}")
target_link_libraries(pae_protocol_lab_ascii_public_a1 PUBLIC PAE::pae pae_lab_features PRIVATE pae::project_options)

add_library(pae_protocol_lab_ui_owned_presentation STATIC
  "${ui_dir}/canonical_input.cpp" "${ui_dir}/canonical_input.h"
  "${ui_dir}/ascii_host_types.cpp" "${ui_dir}/ascii_host_types.h"
  "${ui_dir}/owned_presentation_types.h" "${ui_dir}/ui_physical_types.h")
target_include_directories(pae_protocol_lab_ui_owned_presentation PUBLIC "${ui_dir}")
target_link_libraries(pae_protocol_lab_ui_owned_presentation PUBLIC PAE::pae pae_lab_features PRIVATE pae::project_options)

add_library(pae_protocol_lab_ui_ascii_public_facade STATIC
  "${ui_dir}/ascii_host_adapter.cpp" "${ui_dir}/ascii_host_adapter.h")
target_include_directories(pae_protocol_lab_ui_ascii_public_facade PUBLIC "${ui_dir}")
target_link_libraries(pae_protocol_lab_ui_ascii_public_facade PUBLIC
  pae_protocol_lab_ui_owned_presentation pae_protocol_lab_ascii_public_a1 PAE::pae pae_lab_features
  PRIVATE pae::project_options)

add_library(pae_protocol_lab_ui_binary_public_h2 STATIC
  "${ui_dir}/binary_host_adapter_public.cpp" "${ui_dir}/binary_host_adapter_public.h"
  "${ui_dir}/public_binary_description.cpp" "${ui_dir}/public_binary_description.h")
target_include_directories(pae_protocol_lab_ui_binary_public_h2 PUBLIC "${ui_dir}")
target_link_libraries(pae_protocol_lab_ui_binary_public_h2 PUBLIC
  pae_protocol_lab_binary_public_h1 pae_protocol_lab_ui_owned_presentation pae_lab_features
  PRIVATE pae::project_options)

set(pae_lab_headless_sources
  "${ui_dir}/compile_worker.cpp" "${ui_dir}/compile_worker.h"
  "${ui_dir}/schema_dispatch.cpp" "${ui_dir}/schema_dispatch.h"
  "${ui_dir}/ascii_escaped_input.cpp" "${ui_dir}/ascii_escaped_input.h"
  "${ui_dir}/description_mapping.cpp" "${ui_dir}/description_mapping.h"
  "${ui_dir}/document_session.cpp" "${ui_dir}/document_session.h"
  "${ui_dir}/inspect_hex_input.cpp" "${ui_dir}/inspect_hex_input.h"
  "${ui_dir}/public_legacy_complete_adapter.cpp" "${ui_dir}/public_legacy_complete_adapter.h"
  "${ui_dir}/lab_execution_observer.h"
  "${PAE_LAB_SOURCE_ROOT}/tools/protocol_lab/sha256.cpp"
  "${PAE_LAB_SOURCE_ROOT}/tools/protocol_lab/sha256.h")
add_library(pae_protocol_lab_ui_headless_internal STATIC ${pae_lab_headless_sources})
target_include_directories(pae_protocol_lab_ui_headless_internal PUBLIC "${ui_dir}")
target_link_libraries(pae_protocol_lab_ui_headless_internal PUBLIC
  pae_protocol_lab_ui_owned_presentation pae_protocol_lab_ui_ascii_public_facade
  pae_protocol_lab_ui_binary_public_h2 PAE::pae pae_lab_features
  PRIVATE pae_lab_yyjson_vendor pae::project_options)

add_executable(pae_protocol_lab_ui
  "${ui_dir}/application_window.cpp" "${ui_dir}/application_window.h"
  "${ui_dir}/document_tab.cpp" "${ui_dir}/document_tab.h"
  "${ui_dir}/exact_value_delegate.cpp" "${ui_dir}/exact_value_delegate.h"
  "${ui_dir}/field_table_model.cpp" "${ui_dir}/field_table_model.h"
  "${ui_dir}/hex_view.cpp" "${ui_dir}/hex_view.h" "${ui_dir}/main.cpp")
target_link_libraries(pae_protocol_lab_ui PRIVATE
  pae_protocol_lab_ui_headless_internal PAE::Qt5Widgets pae_lab_features pae::project_options)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/DeployPaeLab.cmake")
pae_lab_configure_deployment(pae_protocol_lab_ui)

set(PAE_BUILD_TESTING ${PAE_LAB_BUILD_TESTING})
set(PAE_LAB_STANDALONE_TEST_SOURCE_ROOT "${PAE_LAB_SOURCE_ROOT}")
set(PAE_LAB_UI_SMOKE_EXECUTABLE "${PAE_LAB_DEPLOY_ROOT}/$<CONFIG>/pae_protocol_lab_ui.exe")
set(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER OFF)
set(PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER OFF)
set(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE ON)
set(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1 ON)
set(PAE_BUILD_PROTOCOL_LAB_BINARY_UI ON)
set(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2 ON)
set(PAE_ENABLE_SCHEMA_V05_COMPILER ON)
set(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER ON)
set(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC ON)
set(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 ON)
set(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 ON)
set(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM ON)
set(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI ON)
set(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER ON)
set(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER ON)
if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER OR PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  target_compile_definitions(pae_lab_features INTERFACE PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1)
endif()
if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER OR PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  target_compile_definitions(pae_lab_features INTERFACE PAE_BUILD_PROTOCOL_LAB_BINDING_UI=1)
endif()
if(PAE_LAB_BUILD_TESTING)
  add_library(pae_protocol_lab_ascii_public_a1_test STATIC ${pae_lab_ascii_public_sources})
  target_include_directories(pae_protocol_lab_ascii_public_a1_test PUBLIC "${ascii_dir}")
  target_compile_definitions(pae_protocol_lab_ascii_public_a1_test
    PUBLIC PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS=1)
  target_link_libraries(pae_protocol_lab_ascii_public_a1_test
    PUBLIC PAE::pae pae_lab_features PRIVATE pae::project_options)
  set(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_TARGET pae_protocol_lab_ascii_public_a1_test)
  add_library(pae_protocol_lab_ui_headless_internal_test STATIC ${pae_lab_headless_sources})
  target_include_directories(pae_protocol_lab_ui_headless_internal_test PUBLIC "${ui_dir}")
  target_compile_definitions(pae_protocol_lab_ui_headless_internal_test
    PUBLIC PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION=1)
  target_link_libraries(pae_protocol_lab_ui_headless_internal_test PUBLIC
    pae_protocol_lab_ui_owned_presentation pae_protocol_lab_ui_ascii_public_facade
    pae_protocol_lab_ui_binary_public_h2 PAE::pae pae_lab_features
    PRIVATE pae_lab_yyjson_vendor pae::project_options)
  set(PAE_PROTOCOL_LAB_UI_HEADLESS_TEST_TARGET pae_protocol_lab_ui_headless_internal_test)
  add_subdirectory("${PAE_LAB_SOURCE_ROOT}/tests/protocol_lab_binary" tests-protocol-lab-binary)
  add_subdirectory("${PAE_LAB_SOURCE_ROOT}/tests/protocol_lab_ascii" tests-protocol-lab-ascii)
  add_subdirectory("${PAE_LAB_SOURCE_ROOT}/tests/protocol_lab_ui" tests-protocol-lab-ui)

endif()
