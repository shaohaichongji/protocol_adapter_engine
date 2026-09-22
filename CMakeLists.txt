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

set(
    PAE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    CACHE STRING
    "MSVC runtime used by PAE targets; Windows SDK packages use /MDd for Debug and /MD for Release"
)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "${PAE_MSVC_RUNTIME_LIBRARY}")
endif()

option(PAE_BUILD_JSON_PARSER_SPIKE "Build the disposable JSON parser comparison spike" OFF)
option(
    PAE_BUILD_PUBLIC_API_STAGE1
    "Build the experimental public compiler, metadata, COMPLETE_RECORD codec, and StreamFramer API"
    OFF
)
option(PAE_BUILD_HOST_ENDPOINT_SLICE "Build the internal serialized host endpoint binding slice" OFF)
option(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER "Build the internal Lab host endpoint observer" OFF)
option(PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER "Build the internal non-Qt Binary candidate DTO slice" OFF)
option(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1 "Build public-only non-Qt Binary complete Decode H1 adapter" OFF)
option(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2 "Route the Qt Lab Binary complete Decode through public PAE" OFF)
option(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE "Route Qt Lab Schema 0.5-0.8 complete records through public PAE" OFF)
option(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 "Build public-only non-Qt ASCII complete-record A1 adapter" OFF)
option(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 "Route Qt Lab ASCII 0.10 through public PAE and Host" OFF)
option(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM "Build public-only non-Qt ASCII 0.11 Host stream adapter" OFF)
option(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI "Route Qt Lab ASCII 0.11 stream through public PAE and Host" OFF)
option(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC "Instrument ASCII Qt UI smoke only in an isolated diagnostic build" OFF)
option(PAE_BUILD_PROTOCOL_LAB_BINARY_UI "Connect the Schema 0.9 Binary Host observer to the Qt Lab" OFF)
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
    PAE_BUILD_PROTOCOL_LAB_UI
    "Build the internal offline Qt Protocol Lab Encode Inspector"
    OFF
)
option(
    PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER
    "Build the internal non-persistent Schema 0.10 ASCII Lab adapter"
    OFF
)
option(
    PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER
    "Build the internal non-persistent Schema 0.11 Lab stream observer"
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
    PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER
    "Enable the internal Schema 0.8 bounded variable COMPLETE_RECORD compiler and Core slice"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING
    "Enable the internal Schema 0.9 bounded stream Framing compiler and Plan slice"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC
    "Enable the internal Schema 0.10 ASCII complete-record compiler and Core slice"
    OFF
)
option(
    PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING
    "Enable the internal Schema 0.11 ASCII CRLF stream framing slice"
    OFF
)
option(
    PAE_BUILD_ASCII_TEXT_EXAMPLE
    "Build the minimal Schema 0.10 ASCII complete-record host example"
    OFF
)
option(
    PAE_BUILD_STREAM_FRAMING_SLICE
    "Build the internal bounded stream Framing execution slice"
    OFF
)
option(
    PAE_BUILD_STREAM_FRAMING_EXAMPLE
    "Build the bounded stream Framing host-push example"
    OFF
)
option(
    PAE_BUILD_ASCII_STREAM_FRAMING_EXAMPLE
    "Build the bounded Schema 0.11 ASCII CRLF stream host example"
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
    PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE
    "Enable the dedicated offline Protocol Lab Schema 0.8 bounded variable command chain"
    OFF
)
option(
    PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE
    "Build the internal minimal business embedding example"
    OFF
)

if(PAE_BUILD_PUBLIC_API_STAGE1)
    foreach(
        pae_public_api_required_option
        IN ITEMS
            PAE_BUILD_LOADER_SCHEMA_IR_SLICE
            PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE
            PAE_ENABLE_SCHEMA_V05_COMPILER
            PAE_ENABLE_SCHEMA_V06_CRC_COMPILER
            PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER
            PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER
            PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING
            PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC
            PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING
            PAE_BUILD_STREAM_FRAMING_SLICE
    )
        if(NOT ${pae_public_api_required_option})
            message(STATUS "PAE_BUILD_PUBLIC_API_STAGE1 enables ${pae_public_api_required_option}.")
            set(
                ${pae_public_api_required_option}
                ON
                CACHE BOOL "Enabled by PAE_BUILD_PUBLIC_API_STAGE1"
                FORCE
            )
        endif()
    endforeach()
    unset(pae_public_api_required_option)
endif()

if(PAE_BUILD_PROTOCOL_LAB_UI AND NOT PAE_BUILD_PUBLIC_API_STAGE1)
    message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_UI requires PAE_BUILD_PUBLIC_API_STAGE1=ON for the Lab-owned presentation targets linking PAE::pae.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1 AND NOT PAE_BUILD_PUBLIC_API_STAGE1)
    message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1 requires PAE_BUILD_PUBLIC_API_STAGE1=ON.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 AND NOT PAE_BUILD_PUBLIC_API_STAGE1)
    message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 requires PAE_BUILD_PUBLIC_API_STAGE1=ON.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM AND
   (NOT PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 OR NOT PAE_BUILD_PUBLIC_API_STAGE1 OR
    NOT PAE_BUILD_HOST_ENDPOINT_SLICE OR NOT PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING))
    message(FATAL_ERROR "ASCII public stream requires public A1, public API Stage 1, Host endpoint and Schema 0.11.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 OR
    NOT PAE_BUILD_PUBLIC_API_STAGE1 OR NOT PAE_BUILD_HOST_ENDPOINT_SLICE))
    message(FATAL_ERROR "ASCII public A2 requires UI, public A1, public API Stage 1 and Host endpoint.")
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 AND NOT PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
    message(
        FATAL_ERROR
        "ASCII public A2 currently requires PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON in this repository's Lab UI build. This is a Lab build-combination constraint; the PAE public API itself does not depend on the private Lab ASCII adapter."
    )
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 OR
    NOT PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM OR
    NOT PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER OR NOT PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER))
    message(FATAL_ERROR "ASCII public stream UI requires UI, public A2/stream, and the existing stream/Host UI surfaces.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC))
    message(FATAL_ERROR "ASCII smoke diagnostic requires Qt Lab UI and Schema 0.10 ASCII text codec.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2 AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_BUILD_PROTOCOL_LAB_BINARY_UI OR
    NOT PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1 OR NOT PAE_BUILD_PUBLIC_API_STAGE1))
    message(FATAL_ERROR "Binary public H2 requires UI, Binary UI, public H1 and public API Stage 1.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_BUILD_PUBLIC_API_STAGE1))
    message(FATAL_ERROR "Public legacy complete migration requires UI and public API Stage 1.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER AND
   (NOT PAE_BUILD_HOST_ENDPOINT_SLICE OR NOT PAE_ENABLE_SCHEMA_V05_COMPILER OR
    NOT PAE_ENABLE_SCHEMA_V06_CRC_COMPILER OR NOT PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER OR
    NOT PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER OR NOT PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING))
    message(FATAL_ERROR "Binary materializer requires explicit Host and V05/V06/V07/V08/V09 options.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI AND NOT PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2 AND
   (NOT PAE_BUILD_PROTOCOL_LAB_UI OR NOT PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER OR
    NOT PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER OR
    NOT PAE_BUILD_HOST_ENDPOINT_SLICE OR NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
    NOT PAE_BUILD_STREAM_FRAMING_SLICE OR NOT PAE_ENABLE_SCHEMA_V05_COMPILER OR
    NOT PAE_ENABLE_SCHEMA_V06_CRC_COMPILER OR NOT PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER OR
    NOT PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER OR NOT PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING))
    message(FATAL_ERROR "Binary Protocol Lab UI requires explicit UI, shared Host-panel observer, Binary materializer, Host, Core, Framing and V05-V09 options.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER AND
   (NOT PAE_BUILD_HOST_ENDPOINT_SLICE OR NOT PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER))
    message(FATAL_ERROR "Lab host observer requires explicit Host endpoint and ASCII stream observer options.")
endif()

if(PAE_BUILD_HOST_ENDPOINT_SLICE AND
   (NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
    NOT PAE_BUILD_STREAM_FRAMING_SLICE OR NOT PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING))
    message(FATAL_ERROR "Host endpoint slice requires explicit Loader, Core, Framing and V11 options.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER AND
   (NOT PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING OR
    NOT PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER OR
    NOT PAE_BUILD_STREAM_FRAMING_SLICE))
    message(FATAL_ERROR
        "Lab ASCII stream observer requires explicit V11, ASCII adapter, and Framing slice options."
    )
endif()

if(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING AND PAE_BUILD_PROTOCOL_LAB)
    message(FATAL_ERROR "Schema 0.11 remains unsupported by Protocol Lab CLI and Evidence.")
endif()

if(PAE_BUILD_PROTOCOL_LAB_UI)
    foreach(
        pae_ui_required_option
        IN ITEMS
            PAE_BUILD_LOADER_SCHEMA_IR_SLICE
            PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE
            PAE_ENABLE_SCHEMA_V05_COMPILER
            PAE_ENABLE_SCHEMA_V06_CRC_COMPILER
            PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER
    )
        if(NOT ${pae_ui_required_option})
            message(STATUS "PAE_BUILD_PROTOCOL_LAB_UI enables ${pae_ui_required_option}.")
            set(${pae_ui_required_option} ON CACHE BOOL "Enabled by PAE_BUILD_PROTOCOL_LAB_UI" FORCE)
        endif()
    endforeach()
    unset(pae_ui_required_option)

    include("${PROJECT_SOURCE_DIR}/cmake/PaeQt513.cmake")
    pae_require_protocol_lab_ui_toolchain()
    pae_import_qt513_widgets()
endif()

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
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER AND PAE_BUILD_PROTOCOL_LAB AND
   NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE)
    message(
        FATAL_ERROR
        "Schema 0.8 Protocol Lab requires explicit PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE=ON."
    )
endif()
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE AND
   (NOT PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH OR
    NOT PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER))
    message(
        FATAL_ERROR
        "PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE requires explicit Schema 0.7 Lab and Schema 0.8 variable compiler slices."
    )
endif()
if(PAE_ENABLE_SCHEMA_V05_COMPILER AND NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE)
    message(FATAL_ERROR "PAE_ENABLE_SCHEMA_V05_COMPILER requires the Loader/SchemaIr slice.")
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER AND
   (NOT PAE_ENABLE_SCHEMA_V05_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V06_CRC_COMPILER requires Loader, Schema 0.5, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER AND
   (NOT PAE_ENABLE_SCHEMA_V06_CRC_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER requires Loader, Schema 0.6, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER AND
   (NOT PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER requires Loader, Schema 0.7, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING AND
   (NOT PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING requires Loader, Schema 0.8, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC AND
   (NOT PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC requires Loader, Schema 0.9, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING AND
   (NOT PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC OR
    NOT PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING OR
    NOT PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    (NOT PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND NOT PAE_BUILD_PUBLIC_API_STAGE1)))
    message(
        FATAL_ERROR
        "PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING requires Schema 0.10, Schema 0.9, Loader, and either Core or the public compiler stage."
    )
endif()
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC AND PAE_BUILD_PROTOCOL_LAB)
    message(
        FATAL_ERROR
        "Schema 0.10 ASCII execution is not connected to Protocol Lab or its evidence formats."
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER AND NOT PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    message(
        FATAL_ERROR
        "PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER requires PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON."
    )
endif()
if(PAE_BUILD_ASCII_TEXT_EXAMPLE AND NOT PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    message(FATAL_ERROR "PAE_BUILD_ASCII_TEXT_EXAMPLE requires the Schema 0.10 ASCII slice.")
endif()
if(PAE_BUILD_STREAM_FRAMING_SLICE AND NOT PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
    message(
        FATAL_ERROR
        "PAE_BUILD_STREAM_FRAMING_SLICE requires PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON."
    )
endif()
if(PAE_BUILD_STREAM_FRAMING_EXAMPLE AND NOT PAE_BUILD_STREAM_FRAMING_SLICE)
    message(FATAL_ERROR "PAE_BUILD_STREAM_FRAMING_EXAMPLE requires the Framing slice.")
endif()
if(PAE_BUILD_ASCII_STREAM_FRAMING_EXAMPLE AND
   (NOT PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING OR
    NOT PAE_BUILD_STREAM_FRAMING_SLICE))
    message(
        FATAL_ERROR
        "PAE_BUILD_ASCII_STREAM_FRAMING_EXAMPLE requires the Schema 0.11 and Framing slices."
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
   PAE_BUILD_PUBLIC_API_STAGE1 OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_PROTOCOL_LAB_UI OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE OR
   PAE_BUILD_STREAM_FRAMING_SLICE OR
   PAE_BUILD_ASCII_TEXT_EXAMPLE)
    add_subdirectory(src/protocol_plan)
endif()

if(PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
   (PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE AND PAE_BUILD_TESTING) OR
   PAE_BUILD_PUBLIC_API_STAGE1 OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_PROTOCOL_LAB_UI OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE OR
   PAE_BUILD_STREAM_FRAMING_SLICE OR
   PAE_BUILD_ASCII_TEXT_EXAMPLE)
    add_subdirectory(src/config_compiler)
endif()

if(PAE_BUILD_STREAM_FRAMING_SLICE)
    add_subdirectory(src/protocol_framing)
endif()

if((PAE_BUILD_LOADER_SCHEMA_IR_SLICE OR
    PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
    PAE_BUILD_PROTOCOL_LAB) AND
   PAE_BUILD_TESTING)
    add_subdirectory(tests/config_compiler)
endif()

if(PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE OR
   PAE_BUILD_PUBLIC_API_STAGE1 OR
   PAE_BUILD_PROTOCOL_LAB OR
   PAE_BUILD_PROTOCOL_LAB_UI OR
   PAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE)
    add_subdirectory(src/protocol_core)
endif()

if(PAE_BUILD_PUBLIC_API_STAGE1)
    add_subdirectory(src/public_api)
    add_subdirectory(examples/public_api_codec)
    add_subdirectory(examples/public_api_framer)
    add_subdirectory(examples/public_api_host)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/public_api)
    endif()
    include("${PROJECT_SOURCE_DIR}/cmake/PaeSdkInstall.cmake")
endif()

if(PAE_BUILD_HOST_ENDPOINT_SLICE)
    add_subdirectory(src/host_endpoint)
    add_subdirectory(examples/host_endpoint)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/host_endpoint)
    endif()
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER OR PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1)
    add_subdirectory(tools/protocol_lab_binary)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/protocol_lab_binary)
    endif()
endif()

if(PAE_BUILD_STREAM_FRAMING_SLICE AND PAE_BUILD_TESTING)
    add_subdirectory(tests/protocol_framing)
endif()

if(PAE_BUILD_STREAM_FRAMING_EXAMPLE)
    add_subdirectory(examples/stream_framing)
endif()
if(PAE_BUILD_ASCII_STREAM_FRAMING_EXAMPLE)
    add_subdirectory(examples/ascii_stream_framing)
endif()
if(PAE_BUILD_ASCII_TEXT_EXAMPLE)
    add_subdirectory(examples/ascii_text)
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

if(PAE_BUILD_PROTOCOL_LAB OR PAE_BUILD_PROTOCOL_LAB_UI)
    add_subdirectory(tools/protocol_lab)
    if(PAE_BUILD_PROTOCOL_LAB AND PAE_BUILD_TESTING)
        add_subdirectory(tests/protocol_lab)
    endif()
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER OR PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1 OR
   PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM)
    add_subdirectory(tools/protocol_lab_ascii)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/protocol_lab_ascii)
    endif()
endif()

if(PAE_BUILD_PROTOCOL_LAB_UI)
    add_subdirectory(tools/protocol_lab_ui)
    if(PAE_BUILD_TESTING)
        add_subdirectory(tests/protocol_lab_ui)
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
