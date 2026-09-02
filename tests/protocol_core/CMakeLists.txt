set(pae_protocol_core_test_data_dir "${CMAKE_CURRENT_BINARY_DIR}/data")
set(pae_protocol_core_golden_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/golden")
set(
    pae_protocol_core_vector_source_dir
    "${pae_protocol_core_golden_source_dir}/synthetic_lab_exchange"
)
set(
    pae_protocol_core_vector_data_dir
    "${pae_protocol_core_test_data_dir}/golden/synthetic_lab_exchange"
)

function(pae_verify_fixed_sha256 file_path expected_sha256)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Synthetic Engine Vector source file is missing: ${file_path}")
    endif()

    file(SHA256 "${file_path}" actual_sha256)
    string(TOUPPER "${actual_sha256}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Synthetic Engine Vector source SHA-256 mismatch: ${file_path}\n"
            "expected=${expected_sha256}\n"
            "actual=${actual_sha256}"
        )
    endif()
endfunction()

file(
    GLOB_RECURSE pae_protocol_core_source_files
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/protocol_core/*.h"
    "${PROJECT_SOURCE_DIR}/src/protocol_core/*.cpp"
)
list(SORT pae_protocol_core_source_files)
foreach(pae_protocol_core_source_file IN LISTS pae_protocol_core_source_files)
    file(READ "${pae_protocol_core_source_file}" pae_protocol_core_source_text)
    string(
        REGEX MATCH
        "(^|[^A-Za-z0-9_])(malloc|calloc|realloc|free)[ \t\r\n]*\\("
        pae_protocol_core_forbidden_heap_call
        "${pae_protocol_core_source_text}"
    )
    if(pae_protocol_core_forbidden_heap_call)
        file(
            RELATIVE_PATH pae_protocol_core_source_relative
            "${PROJECT_SOURCE_DIR}"
            "${pae_protocol_core_source_file}"
        )
        message(
            FATAL_ERROR
            "Protocol Core source contains forbidden C heap call '${CMAKE_MATCH_2}': "
            "${pae_protocol_core_source_relative}"
        )
    endif()
endforeach()

file(MAKE_DIRECTORY "${pae_protocol_core_test_data_dir}/golden")
file(MAKE_DIRECTORY "${pae_protocol_core_vector_data_dir}")

set(
    pae_request_frame
    "${pae_protocol_core_vector_source_dir}/lab_command_001.frame.hex"
)
set(
    pae_request_decode_expected
    "${pae_protocol_core_vector_source_dir}/lab_command_001.decode_expected.tsv"
)
set(
    pae_request_encode_input
    "${pae_protocol_core_vector_source_dir}/lab_command_001.encode_input.tsv"
)
set(
    pae_response_frame
    "${pae_protocol_core_vector_source_dir}/lab_report_001.frame.hex"
)
set(
    pae_response_decode_expected
    "${pae_protocol_core_vector_source_dir}/lab_report_001.decode_expected.tsv"
)
set(
    pae_response_encode_input
    "${pae_protocol_core_vector_source_dir}/lab_report_001.encode_input.tsv"
)

pae_verify_fixed_sha256(
    "${pae_request_frame}"
    "492AF67348459F1450FC87287B5CC6907D6FF5E75A295DBA36B489E09909040D"
)
pae_verify_fixed_sha256(
    "${pae_request_decode_expected}"
    "55F98CD3BF4DA09D0228781AE7F24E9843B9DA0F10FCAE04CEAF268D974B641F"
)
pae_verify_fixed_sha256(
    "${pae_request_encode_input}"
    "714D63AC19391AA8F52A31EF0BB6C7B84291FAA4BCF723B096DC202F06856DD3"
)
pae_verify_fixed_sha256(
    "${pae_response_frame}"
    "AB2C014349E0C673FBF38C692DC2B5F1CB70609CF00968D94449A848E3B24E31"
)
pae_verify_fixed_sha256(
    "${pae_response_decode_expected}"
    "3734FA466D7412FF007880873FC9F9A5AACB4513A0837B3D36A4AD14D25E8CE5"
)
pae_verify_fixed_sha256(
    "${pae_response_encode_input}"
    "69BCD63709A6236C120B7072D28040035BCC3EF43B13C8A2F15E248216681246"
)

configure_file(
    "${pae_protocol_core_golden_source_dir}/manifest_v0.1.tsv"
    "${pae_protocol_core_test_data_dir}/golden/manifest_v0.1.tsv"
    COPYONLY
)
configure_file(
    "${pae_request_frame}"
    "${pae_protocol_core_vector_data_dir}/lab_command_001.frame.hex"
    COPYONLY
)
configure_file(
    "${pae_request_decode_expected}"
    "${pae_protocol_core_vector_data_dir}/lab_command_001.decode_expected.tsv"
    COPYONLY
)
configure_file(
    "${pae_request_encode_input}"
    "${pae_protocol_core_vector_data_dir}/lab_command_001.encode_input.tsv"
    COPYONLY
)
configure_file(
    "${pae_response_frame}"
    "${pae_protocol_core_vector_data_dir}/lab_report_001.frame.hex"
    COPYONLY
)
configure_file(
    "${pae_response_decode_expected}"
    "${pae_protocol_core_vector_data_dir}/lab_report_001.decode_expected.tsv"
    COPYONLY
)
configure_file(
    "${pae_response_encode_input}"
    "${pae_protocol_core_vector_data_dir}/lab_report_001.encode_input.tsv"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
    "${pae_protocol_core_test_data_dir}/synthetic_lab_exchange_slice.pae.json"
    COPYONLY
)

add_executable(
    pae_complete_record_codec_contract_tests
    complete_record_codec_contract_tests.cpp
    fixture_reader.cpp
    fixture_reader.h
)

target_include_directories(
    pae_complete_record_codec_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
)

target_link_libraries(
    pae_complete_record_codec_contract_tests
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
)

add_test(
    NAME pae.protocol_core.complete_record_codec.contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_TEST_EXECUTABLE=$<TARGET_FILE:pae_complete_record_codec_contract_tests>"
        "-DPAE_TEST_DATA_DIR=${pae_protocol_core_test_data_dir}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_golden_files.cmake"
)

set_tests_properties(
    pae.protocol_core.complete_record_codec.contract
    PROPERTIES
        LABELS "protocol_core;complete_record_codec"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(
    pae_complete_record_first_call_allocation_tests
    first_call_allocation_tests.cpp
)

target_link_libraries(
    pae_complete_record_first_call_allocation_tests
    PRIVATE
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
)

add_test(
    NAME pae.protocol_core.complete_record_codec.first_decode_allocation
    COMMAND pae_complete_record_first_call_allocation_tests --first-decode
)
add_test(
    NAME pae.protocol_core.complete_record_codec.first_encode_allocation
    COMMAND pae_complete_record_first_call_allocation_tests --first-encode
)

set_tests_properties(
    pae.protocol_core.complete_record_codec.first_decode_allocation
    pae.protocol_core.complete_record_codec.first_encode_allocation
    PROPERTIES
        LABELS "protocol_core;complete_record_codec;allocation"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(
    pae_complete_record_operation_count_tests
    operation_count_tests.cpp
)

target_link_libraries(
    pae_complete_record_operation_count_tests
    PRIVATE
        pae_protocol_core_slice_instrumented
        pae::protocol_plan
        pae::project_options
)

add_test(
    NAME pae.protocol_core.complete_record_codec.operation_count
    COMMAND pae_complete_record_operation_count_tests
)

set_tests_properties(
    pae.protocol_core.complete_record_codec.operation_count
    PROPERTIES
        LABELS "protocol_core;complete_record_codec;instrumentation"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Threads REQUIRED)

add_executable(
    pae_complete_record_shared_plan_concurrency_tests
    shared_plan_concurrency_tests.cpp
)

target_link_libraries(
    pae_complete_record_shared_plan_concurrency_tests
    PRIVATE
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
        Threads::Threads
)

add_test(
    NAME pae.protocol_core.complete_record_codec.shared_plan_concurrency
    COMMAND pae_complete_record_shared_plan_concurrency_tests
)

set_tests_properties(
    pae.protocol_core.complete_record_codec.shared_plan_concurrency
    PROPERTIES
        LABELS "protocol_core;complete_record_codec;concurrency"
        PROCESSORS 4
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
