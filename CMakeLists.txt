cmake_minimum_required(VERSION 3.25...4.4)

project(
    ProtocolAdapterEngine
    VERSION 0.1.0
    DESCRIPTION "Configuration-driven industrial binary protocol adapter engine"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PAE_BUILD_JSON_PARSER_SPIKE "Build the disposable JSON parser comparison spike" OFF)
option(
    PAE_BUILD_LOADER_SCHEMA_IR_SLICE
    "Build the internal Loader/SchemaIr executable vertical slice"
    OFF
)
option(
    PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE
    "Build the internal COMPLETE_RECORD Decode/Encode executable vertical slice"
    OFF
)
option(
    PAE_BUILD_PROTOCOL_LAB
    "Build the internal offline Protocol Lab command-line tool"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V05_COMPILER
    "Enable the internal Schema 0.5 compiler and Core DEC-042B slices"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V06_CRC_COMPILER
    "Enable the internal Schema 0.6 parameterized CRC compiler and Core slice"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER
    "Enable the internal Schema 0.7 fixed-record computed length compiler and Core slice"
    OFF
)
option(
    PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05
    "Enable the dedicated offline Protocol Lab Schema 0.5 C3 command chain"
    OFF
)
option(
    PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC
    "Enable the dedicated offline Protocol Lab Schema 0.6 CRC command chain"
    OFF
)
option(
    PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH
    "Enable the dedicated offline Protocol Lab Schema 0.7 length command chain"
    OFF
)
option(
    PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE
    "Build the internal minimal business embedding example"
    OFF
)

if(PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE AND NOT PAE_ENABLE_SCHEMA_V05_COMPILER)
    message(
        FATAL_ERROR
        "PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE requires PAE_ENABLE_SCHEMA_V05_COMPILER=ON."
    )
endif()

if(PAE_ENABLE_SCHEMA_V05_COMPILER AND PAE_BUILD_PROTOCOL_LAB AND
   NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05)
    message(
        FATAL_ERROR
        "Schema 0.5 Protocol Lab requires explicit PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05=ON."
    )
endif()
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05 AND
   (NOT PAE_BUILD_PROTOCOL_LAB OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
    NOT PAE_ENABLE_SCHEMA_V05_COMPILER))
    message(
        FATAL_ERROR
        "PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05 requires Protocol Lab, Loader, Core, and Schema 0.5 compiler slices."
    )
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER AND PAE_BUILD_PROTOCOL_LAB AND
   NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
    message(
        FATAL_ERROR
        "Schema 0.6 Protocol Lab requires explicit PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC=ON."
    )
endif()
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC AND
   (NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05 OR
    NOT PAE_ENABLE_SCHEMA_V06_CRC_COMPILER))
    message(
        FATAL_ERROR
        "PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC requires explicit Schema 0.5 Lab and Schema 0.6 CRC compiler slices."
    )
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER AND PAE_BUILD_PROTOCOL_LAB AND
   NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
    message(
        FATAL_ERROR
        "Schema 0.7 Protocol Lab requires explicit PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH=ON."
    )
endif()
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH AND
   (NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC OR
    NOT PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER))
    message(
        FATAL_ERROR
        "PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH requires explicit Schema 0.6 CRC Lab and Schema 0.7 length compiler slices."
    )
endif()
if(PAE_ENABLE_SCHEMA_V05_COMPILER AND NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE)
    message(FATAL_ERROR "PAE_ENABLE_SCHEMA_V05_COMPILER requires the Loader/SchemaIr slice.")
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER AND
   (NOT PAE_ENABLE_SCHEMA_V05_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V06_CRC_COMPILER requires the Loader, Core, and explicit Schema 0.5 compiler slices."
    )
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER AND
   (NOT PAE_ENABLE_SCHEMA_V06_CRC_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER requires the Loader, Core, and explicit Schema 0.6 CRC compiler slice."
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    set(pae_build_testing_default "${BUILD_TESTING}")
else()
    set(pae_build_testing_default OFF)
endif()
option(
    PAE_BUILD_TESTING
    "Build ProtocolAdapterEngine tests and test-only instrumentation"
    "${pae_build_testing_default}"
)
if(PAE_BUILD_TESTING)
    enable_testing()
endif()
unset(pae_build_testing_default)

option(
    PAE_BUILD_LAB_V06_FORMAT_TESTS
    "Build the isolated Protocol Lab Result 0.6 pure-format tests"
    OFF
)
if(PAE_BUILD_LAB_V06_FORMAT_TESTS AND NOT PAE_BUILD_TESTING)
    message(FATAL_ERROR "PAE_BUILD_LAB_V06_FORMAT_TESTS requires PAE_BUILD_TESTING=ON.")
endif()

option(
    PAE_BUILD_LAB_V06_EVIDENCE_TESTS
    "Build the isolated Protocol Lab Evidence Bundle 0.6 tests"
    OFF
)
if(PAE_BUILD_LAB_V06_EVIDENCE_TESTS AND NOT PAE_BUILD_TESTING)
    message(FATAL_ERROR "PAE_BUILD_LAB_V06_EVIDENCE_TESTS requires PAE_BUILD_TESTING=ON.")
endif()

option(
    PAE_BUILD_LAB_V06_EXECUTION_TESTS
    "Build the isolated Protocol Lab Schema 0.5 execution bridge tests"
    OFF
)
if(PAE_BUILD_LAB_V06_EXECUTION_TESTS)
    if(NOT PAE_BUILD_TESTING)
        message(FATAL_ERROR "PAE_BUILD_LAB_V06_EXECUTION_TESTS requires PAE_BUILD_TESTING=ON.")
    endif()
    if(NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
       NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
       NOT PAE_ENABLE_SCHEMA_V05_COMPILER)
        message(
            FATAL_ERROR
            "PAE_BUILD_LAB_V06_EXECUTION_TESTS requires the Loader, Core, and Schema 0.5 compiler slices."
        )
    endif()
endif()

option(
    PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS
    "Build the isolated Protocol Lab C execution Evidence 0.7 tests"
    OFF
)
if(PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS)
    if(NOT PAE_BUILD_TESTING)
        message(FATAL_ERROR "PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS requires PAE_BUILD_TESTING=ON.")
    endif()
    if(NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
       NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
       NOT PAE_ENABLE_SCHEMA_V05_COMPILER)
        message(
            FATAL_ERROR
            "PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS requires the Loader, Core, and Schema 0.5 compiler slices."
        )
    endif()
endif()

add_library(pae_project_options INTERFACE)
add_library(pae::project_options ALIAS pae_project_options)
target_compile_features(pae_project_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(
        pae_project_options
        INTERFACE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
    )
else()
    target_compile_options(
        pae_project_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
endif()

if(PAE_BUILD_JSON_PARSER_SPIKE)
    add_subdirectory(spikes/json_parser)
endif()

if(PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
   PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE)
    add_subdirectory(src/protocol_plan)
endif()

if(PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
   (PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND PAE_BUILD_TESTING) OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE)
    add_subdirectory(src/config_compiler)
endif()

if((PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
    PAE_BUILD_PROTOCOL_LAB) AND
   PAE_BUILD_TESTING)
    add_subdirectory(tests/config_compiler)
endif()

if(PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE)
    add_subdirectory(src/protocol_core)
endif()

if(PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE)
    add_subdirectory(examples/business_embedding)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/business_embedding)
    endif()
endif()

if(PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND PAE_BUILD_TESTING)
    add_subdirectory(tests/protocol_core)
    add_subdirectory(tools/protocol_conformance_runner)
endif()

if(PAE_BUILD_PROTOCOL_LAB)
    add_subdirectory(tools/protocol_lab)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/protocol_lab)
    endif()
endif()

if(PAE_BUILD_LAB_V06_FORMAT_TESTS)
    add_subdirectory(tests/protocol_lab_v06_format)
endif()

if(PAE_BUILD_LAB_V06_EVIDENCE_TESTS)
    add_subdirectory(tests/protocol_lab_v06_evidence)
endif()

if(PAE_BUILD_LAB_V06_EXECUTION_TESTS)
    add_subdirectory(tests/protocol_lab_v06_execution)
endif()

if(PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS)
    add_subdirectory(tests/protocol_lab_v07_run_evidence)
endif()
if(PAE_BUILD_TESTING AND PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05)
    add_subdirectory(tests/protocol_lab_c3_cli)
endif()

if(PAE_BUILD_TESTING AND TARGET pae_yyjson_vendor)
    add_test(
        NAME pae.dependency.yyjson.vendor_contract
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPAE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DPAE_BINARY_DIR=${PROJECT_BINARY_DIR}/tests/dependency_contract"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyYyjsonVendorContract.cmake"
    )
    set_tests_properties(
        pae.dependency.yyjson.vendor_contract
        PROPERTIES
            LABELS "dependency;offline;yyjson"
            TIMEOUT 30
    )
endif()
