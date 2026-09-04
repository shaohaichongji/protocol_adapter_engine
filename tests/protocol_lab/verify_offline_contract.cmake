cmake_minimum_required(VERSION 3.25)

if(NOT EXISTS "${PAE_LAB_EXECUTABLE}")
    message(FATAL_ERROR "Protocol Lab executable is missing: ${PAE_LAB_EXECUTABLE}")
endif()

set(pae_lab_config "${PAE_LAB_DATA_DIR}/synthetic_lab_exchange_slice.pae.json")
set(pae_lab_frame "${PAE_LAB_DATA_DIR}/lab_command_001.frame.hex")
set(pae_lab_other_frame "${PAE_LAB_DATA_DIR}/lab_report_001.frame.hex")
set(pae_lab_values "${PAE_LAB_DATA_DIR}/valid_command.values.pae-lab.json")

file(REMOVE_RECURSE "${PAE_LAB_RUN_ROOT}")
file(MAKE_DIRECTORY "${PAE_LAB_RUN_ROOT}")

function(pae_run expected_exit output_variable error_variable)
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE pae_actual_exit
        OUTPUT_VARIABLE pae_stdout
        ERROR_VARIABLE pae_stderr
        ENCODING UTF-8
    )
    if(NOT pae_actual_exit EQUAL expected_exit)
        message(
            FATAL_ERROR
            "Protocol Lab exit mismatch: expected=${expected_exit} actual=${pae_actual_exit}\n"
            "command=${ARGN}\nstdout=${pae_stdout}\nstderr=${pae_stderr}"
        )
    endif()
    set(${output_variable} "${pae_stdout}" PARENT_SCOPE)
    set(${error_variable} "${pae_stderr}" PARENT_SCOPE)
endfunction()

function(pae_expect_contains text pattern description)
    string(FIND "${text}" "${pattern}" pae_found)
    if(pae_found EQUAL -1)
        message(FATAL_ERROR "${description}: missing '${pattern}' in '${text}'")
    endif()
endfunction()

function(pae_verify_bundle bundle)
    foreach(pae_required IN ITEMS
            inputs/protocol.pae.json
            frames/000001_frame.bin
            frames/000001_frame.hex
            run_record_v0.1.json
            events_v0.1.jsonl
            result_summary_v0.1.json
            SHA256SUMS
            COMPLETE)
        if(NOT EXISTS "${bundle}/${pae_required}")
            message(FATAL_ERROR "Evidence Bundle is missing ${pae_required}: ${bundle}")
        endif()
    endforeach()
    file(STRINGS "${bundle}/SHA256SUMS" pae_hash_lines)
    file(STRINGS "${bundle}/events_v0.1.jsonl" pae_event_lines)
    list(LENGTH pae_event_lines pae_event_line_count)
    if(NOT pae_event_line_count EQUAL 1)
        message(FATAL_ERROR "V0.1 single-event JSONL must contain exactly one line")
    endif()
    set(pae_previous_path "")
    foreach(pae_line IN LISTS pae_hash_lines)
        if(NOT pae_line MATCHES "^([0-9a-f]+)  (.+)$")
            message(FATAL_ERROR "Invalid SHA256SUMS line: ${pae_line}")
        endif()
        set(pae_expected "${CMAKE_MATCH_1}")
        set(pae_path "${CMAKE_MATCH_2}")
        string(LENGTH "${pae_expected}" pae_hash_length)
        if(NOT pae_hash_length EQUAL 64)
            message(FATAL_ERROR "Invalid SHA-256 length in line: ${pae_line}")
        endif()
        if(NOT pae_previous_path STREQUAL "" AND NOT pae_previous_path STRLESS pae_path)
            message(FATAL_ERROR "SHA256SUMS is not strictly path-sorted")
        endif()
        if(NOT EXISTS "${bundle}/${pae_path}")
            message(FATAL_ERROR "SHA256SUMS references missing file: ${pae_path}")
        endif()
        file(SHA256 "${bundle}/${pae_path}" pae_actual)
        if(NOT pae_actual STREQUAL pae_expected)
            message(FATAL_ERROR "SHA-256 mismatch for ${pae_path}")
        endif()
        set(pae_previous_path "${pae_path}")
    endforeach()
endfunction()

pae_run(
    0 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-hex "${pae_lab_frame}" --output json
)
pae_expect_contains("${pae_output}" "\"operation_status\":\"OK\"" "inspect status")
pae_expect_contains("${pae_output}" "\"message_id\":\"lab_command\"" "inspect message")
pae_expect_contains("${pae_output}" "0D00A731563412DEADBEEF0200" "inspect canonical frame")

pae_run(
    0 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-hex "${PAE_LAB_DATA_DIR}/unknown.frame.hex"
    --expect-status UNKNOWN_MESSAGE --output json
)
pae_expect_contains("${pae_output}" "\"operation_status\":\"UNKNOWN_MESSAGE\"" "expected status")

pae_run(
    0 pae_output pae_error
    inspect --config "${PAE_LAB_DATA_DIR}/ambiguous.pae.json"
    --frame-hex "${PAE_LAB_DATA_DIR}/ambiguous.frame.hex"
    --expect-status AMBIGUOUS_MESSAGE --output json
)
pae_expect_contains("${pae_output}" "\"operation_status\":\"AMBIGUOUS_MESSAGE\"" "ambiguity")

pae_run(
    3 pae_output pae_error
    inspect --config "${pae_lab_config}"
    --frame-hex "${PAE_LAB_DATA_DIR}/invalid_syntax.frame.hex" --output json
)

pae_run(
    5 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-hex "${PAE_LAB_DATA_DIR}/unknown.frame.hex"
    --output json
)

pae_run(
    0 pae_output pae_error
    encode --config "${pae_lab_config}" --values "${pae_lab_values}" --output json
)
pae_expect_contains("${pae_output}" "0D00A731563412DEADBEEF0200" "encode exact bytes")

foreach(pae_invalid IN ITEMS
        invalid_missing
        invalid_duplicate
        invalid_type
        invalid_overflow
        invalid_constant_override)
    pae_run(
        5 pae_output pae_error
        encode --config "${pae_lab_config}"
        --values "${PAE_LAB_DATA_DIR}/${pae_invalid}.values.pae-lab.json" --output json
    )
endforeach()

pae_run(
    0 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-hex "${pae_lab_frame}"
    --record-root "${PAE_LAB_RUN_ROOT}" --output json
)
file(GLOB pae_runs LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(LENGTH pae_runs pae_run_count)
if(NOT pae_run_count EQUAL 1)
    message(FATAL_ERROR "Expected one completed inspect Run, found ${pae_run_count}")
endif()
list(GET pae_runs 0 pae_original_run)
pae_verify_bundle("${pae_original_run}")

pae_run(
    0 pae_output pae_error
    replay --bundle "${pae_original_run}" --record-root "${PAE_LAB_RUN_ROOT}" --output json
)
pae_expect_contains("${pae_output}" "\"comparison_equal\":true" "replay comparison")
file(GLOB pae_runs LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(LENGTH pae_runs pae_run_count)
if(NOT pae_run_count EQUAL 2)
    message(FATAL_ERROR "Replay did not create one new immutable Run")
endif()
foreach(pae_run IN LISTS pae_runs)
    if(NOT pae_run STREQUAL pae_original_run)
        set(pae_replay_run "${pae_run}")
    endif()
endforeach()
pae_verify_bundle("${pae_replay_run}")

pae_run(
    0 pae_output pae_error
    replay --bundle "${pae_original_run}" --config "${pae_lab_config}"
    --record-root "${PAE_LAB_RUN_ROOT}" --output json
)
pae_expect_contains("${pae_output}" "\"cross_config_replay\":true" "cross-config replay marker")

set(pae_encode_root "${PAE_LAB_RUN_ROOT}-encode")
file(REMOVE_RECURSE "${pae_encode_root}")
file(MAKE_DIRECTORY "${pae_encode_root}")
pae_run(
    0 pae_output pae_error
    encode --config "${pae_lab_config}" --values "${pae_lab_values}"
    --record-root "${pae_encode_root}" --output json
)
file(GLOB pae_encode_runs LIST_DIRECTORIES true "${pae_encode_root}/run_*")
list(LENGTH pae_encode_runs pae_encode_run_count)
if(NOT pae_encode_run_count EQUAL 1)
    message(FATAL_ERROR "Expected one completed encode Run")
endif()
list(GET pae_encode_runs 0 pae_encode_run)
if(NOT EXISTS "${pae_encode_run}/inputs/values.pae-lab.json")
    message(FATAL_ERROR "Encode Evidence Bundle did not preserve Values input")
endif()
pae_verify_bundle("${pae_encode_run}")
pae_run(
    0 pae_output pae_error
    replay --bundle "${pae_encode_run}" --record-root "${pae_encode_root}" --output json
)
pae_expect_contains("${pae_output}" "\"comparison_equal\":true" "encode replay comparison")

pae_run(
    0 pae_output pae_error
    compare --left-run "${pae_original_run}" --right-run "${pae_replay_run}" --output json
)
pae_expect_contains("${pae_output}" "\"operation_status\":\"EQUAL\"" "Run compare")
pae_run(
    6 pae_output pae_error
    compare --left-run "${pae_original_run}" --right-run "${pae_encode_run}" --output json
)
pae_expect_contains("${pae_output}" "OPERATION_KIND" "Run difference category")

pae_run(
    0 pae_output pae_error
    compare --left-frame-hex "${pae_lab_frame}" --right-frame-hex "${pae_lab_frame}"
)
pae_run(
    0 pae_output pae_error
    compare --left-frame-hex "${pae_lab_frame}"
    --right-frame-bin "${pae_original_run}/frames/000001_frame.bin"
)
pae_run(
    6 pae_output pae_error
    compare --left-frame-hex "${pae_lab_frame}" --right-frame-hex "${pae_lab_other_frame}"
    --output json
)
pae_expect_contains("${pae_output}" "WIRE_BYTES" "Frame difference category")

pae_run(2 pae_output pae_error unknown-command)
pae_run(
    3 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-bin "${PAE_LAB_DATA_DIR}/missing.bin"
)
pae_run(
    4 pae_output pae_error
    inspect --config "${PAE_LAB_DATA_DIR}/invalid_config.pae.json" --frame-hex "${pae_lab_frame}"
)

file(WRITE "${PAE_LAB_RUN_ROOT}/not-a-directory" "record failure probe")
pae_run(
    7 pae_output pae_error
    inspect --config "${pae_lab_config}" --frame-hex "${pae_lab_frame}"
    --record-root "${PAE_LAB_RUN_ROOT}/not-a-directory"
)

set(pae_tamper_parent "${PAE_LAB_RUN_ROOT}-tampered")
file(REMOVE_RECURSE "${pae_tamper_parent}")
file(MAKE_DIRECTORY "${pae_tamper_parent}")
file(COPY "${pae_original_run}" DESTINATION "${pae_tamper_parent}")
get_filename_component(pae_original_name "${pae_original_run}" NAME)
set(pae_tampered_run "${pae_tamper_parent}/${pae_original_name}")
file(APPEND "${pae_tampered_run}/frames/000001_frame.bin" "tampered")
pae_run(3 pae_output pae_error replay --bundle "${pae_tampered_run}" --output json)
pae_expect_contains("${pae_output}" "SHA-256 mismatch" "tampered Bundle rejection")

set(pae_extra_parent "${PAE_LAB_RUN_ROOT}-extra-file")
file(REMOVE_RECURSE "${pae_extra_parent}")
file(MAKE_DIRECTORY "${pae_extra_parent}")
file(COPY "${pae_original_run}" DESTINATION "${pae_extra_parent}")
set(pae_extra_run "${pae_extra_parent}/${pae_original_name}")
file(WRITE "${pae_extra_run}/unlisted.txt" "must be rejected")
pae_run(3 pae_output pae_error replay --bundle "${pae_extra_run}" --output json)
pae_expect_contains("${pae_output}" "unlisted file" "unlisted Bundle file rejection")

pae_run(
    2 pae_output pae_error
    inspect --config "${pae_lab_config}" --config "${pae_lab_config}"
    --frame-hex "${pae_lab_frame}"
)

file(GLOB_RECURSE pae_inprogress "${PAE_LAB_RUN_ROOT}/*.inprogress")
if(pae_inprogress)
    message(FATAL_ERROR "Successful contract run left unexpected .inprogress directories")
endif()

message(STATUS "Protocol Lab offline contract checks passed")
