cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS PAE_LAB_EXECUTABLE PAE_SOURCE_DIR PAE_BINARY_DIR PAE_FIXTURE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(config "${PAE_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json")
set(runs "${PAE_BINARY_DIR}/runs")
file(RELATIVE_PATH runs_cli "${PAE_SOURCE_DIR}" "${runs}")
file(REMOVE_RECURSE "${runs}")
file(MAKE_DIRECTORY "${runs}")

function(run_json name expected_exit)
    set(command_arguments)
    foreach(argument IN LISTS ARGN)
        if(IS_ABSOLUTE "${argument}")
            file(RELATIVE_PATH relative_argument "${PAE_SOURCE_DIR}" "${argument}")
            if(NOT relative_argument MATCHES "^\\.\\.")
                list(APPEND command_arguments "${relative_argument}")
                continue()
            endif()
        endif()
        list(APPEND command_arguments "${argument}")
    endforeach()
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${command_arguments}
        WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE output
        ERROR_VARIABLE stderr
    )
    file(WRITE "${PAE_BINARY_DIR}/${name}.json" "${output}")
    file(WRITE "${PAE_BINARY_DIR}/${name}.stderr.txt" "${stderr}")
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: expected exit ${expected_exit}, got ${actual_exit}\n${output}\n${stderr}")
    endif()
    string(JSON format ERROR_VARIABLE json_error GET "${output}" format_version)
    if(NOT format STREQUAL "pae.lab.cli/0.1")
        message(FATAL_ERROR "${name}: invalid C3 envelope: ${json_error}\n${output}")
    endif()
    string(JSON declared_exit GET "${output}" process_exit_code)
    if(NOT declared_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: envelope exit ${declared_exit} differs from process ${expected_exit}")
    endif()
    set(${name}_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(resolve_cli_path variable)
    if(NOT IS_ABSOLUTE "${${variable}}")
        get_filename_component(resolved "${${variable}}" ABSOLUTE BASE_DIR "${PAE_SOURCE_DIR}")
        set(${variable} "${resolved}" PARENT_SCOPE)
    endif()
endfunction()

function(json_expect json path expected)
    string(REPLACE "/" ";" parts "${path}")
    string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${parts})
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${path}: expected '${expected}', got '${actual}' (${json_error})")
    endif()
endfunction()

run_json(
    encode 0 encode --config "${config}"
    --values "${PAE_FIXTURE_DIR}/valid.values.pae-lab.json"
    --record-root "${runs}" --output json
)
json_expect("${encode_OUTPUT}" "command" "encode")
json_expect("${encode_OUTPUT}" "current_terminal_status" "OK")
json_expect("${encode_OUTPUT}" "result/format_version" "pae.lab.result/0.6")
json_expect("${encode_OUTPUT}" "result/frame_hex"
            "000000000000020BFFFFFFFFFFFFFFFF00000000000000008000000000000000A52A")
string(JSON encode_bundle GET "${encode_OUTPUT}" published_bundle)
resolve_cli_path(encode_bundle)

run_json(
    inspect 0 inspect --config "${config}"
    --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex"
    --record-root "${runs}" --output json
)
json_expect("${inspect_OUTPUT}" "result/fields/0/decimal64/coefficient" "123")
json_expect("${inspect_OUTPUT}" "result/fields/0/decimal64/scale" "1")
string(JSON inspect_bundle GET "${inspect_OUTPUT}" published_bundle)
resolve_cli_path(inspect_bundle)

run_json(replay_a 0 replay --bundle "${inspect_bundle}" --record-root "${runs}" --output json)
json_expect("${replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${replay_a_OUTPUT}" "result/replay_mode" "DECODE_RX")
string(JSON replay_a_bundle GET "${replay_a_OUTPUT}" published_bundle)
resolve_cli_path(replay_a_bundle)
run_json(replay_b 0 replay --bundle "${replay_a_bundle}" --record-root "${runs}" --output json)
json_expect("${replay_b_OUTPUT}" "comparison/status" "EQUAL")

file(GLOB before_compare_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH before_compare_runs before_compare_count)
run_json(compare_equal 0 compare --left-run "${inspect_bundle}" --right-run "${replay_a_bundle}"
         --output json)
file(GLOB after_compare_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH after_compare_runs after_compare_count)
if(NOT before_compare_count EQUAL after_compare_count)
    message(FATAL_ERROR "compare published an unexpected Bundle")
endif()
json_expect("${compare_equal_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${compare_equal_OUTPUT}" "result" "")

run_json(
    equivalent 0 encode --config "${config}"
    --values "${PAE_FIXTURE_DIR}/equivalent.values.pae-lab.json"
    --record-root "${runs}" --output json
)
string(JSON equivalent_bundle GET "${equivalent_OUTPUT}" published_bundle)
resolve_cli_path(equivalent_bundle)
run_json(compare_equivalent 0 compare --left-run "${encode_bundle}"
         --right-run "${equivalent_bundle}" --output json)
json_expect("${compare_equivalent_OUTPUT}" "comparison/status" "EQUAL")

run_json(
    different 0 encode --config "${config}"
    --values "${PAE_FIXTURE_DIR}/different.values.pae-lab.json"
    --record-root "${runs}" --output json
)
string(JSON different_bundle GET "${different_OUTPUT}" published_bundle)
resolve_cli_path(different_bundle)
run_json(compare_different 6 compare --left-run "${encode_bundle}"
         --right-run "${different_bundle}" --output json)
json_expect("${compare_different_OUTPUT}" "comparison/status" "DIFFERENT")
json_expect("${compare_different_OUTPUT}" "diagnostic/id" "PAE_LAB_C3_COMPARE_DIFFERENT")

run_json(
    codec_failure 5 encode --config "${config}"
    --values "${PAE_FIXTURE_DIR}/failing.values.pae-lab.json"
    --record-root "${runs}" --output json
)
json_expect("${codec_failure_OUTPUT}" "current_terminal_status" "CODEC_ERROR")
json_expect("${codec_failure_OUTPUT}" "result/current_execution_status" "VALUE_NOT_REPRESENTABLE")
json_expect("${codec_failure_OUTPUT}" "result/exit_code" "5")
string(JSON failure_fingerprint GET "${codec_failure_OUTPUT}" result deterministic_fingerprint)
string(JSON failure_bundle GET "${codec_failure_OUTPUT}" published_bundle)
resolve_cli_path(failure_bundle)

run_json(
    codec_failure_expected 0 encode --config "${config}"
    --values "${PAE_FIXTURE_DIR}/failing.values.pae-lab.json"
    --record-root "${runs}" --expect-status VALUE_NOT_REPRESENTABLE --output json
)
json_expect("${codec_failure_expected_OUTPUT}" "expectation/matched" "ON")
json_expect("${codec_failure_expected_OUTPUT}" "result/operation_status" "CODEC_ERROR")
json_expect("${codec_failure_expected_OUTPUT}" "result/exit_code" "5")
string(JSON expected_fingerprint GET "${codec_failure_expected_OUTPUT}" result deterministic_fingerprint)
if(NOT failure_fingerprint STREQUAL expected_fingerprint)
    message(FATAL_ERROR "--expect-status changed the Result fingerprint")
endif()
run_json(failure_replay_default 5 replay --bundle "${failure_bundle}" --record-root "${runs}"
         --output json)
json_expect("${failure_replay_default_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${failure_replay_default_OUTPUT}" "result/operation_status" "CODEC_ERROR")
run_json(failure_replay_expected 0 replay --bundle "${failure_bundle}" --record-root "${runs}"
         --expect-status VALUE_NOT_REPRESENTABLE --output json)
json_expect("${failure_replay_expected_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${failure_replay_expected_OUTPUT}" "expectation/matched" "ON")
json_expect("${failure_replay_expected_OUTPUT}" "result/operation_status" "CODEC_ERROR")
json_expect("${failure_replay_expected_OUTPUT}" "result/exit_code" "5")

run_json(
    integrity_failure 5 inspect --config "${config}"
    --frame-hex "${PAE_FIXTURE_DIR}/bad_sum8.frame.hex"
    --record-root "${runs}" --output json
)
json_expect("${integrity_failure_OUTPUT}" "result/current_execution_status" "INTEGRITY_FAILED")
run_json(
    integrity_expected 0 inspect --config "${config}"
    --frame-hex "${PAE_FIXTURE_DIR}/bad_sum8.frame.hex"
    --record-root "${runs}" --expect-status INTEGRITY_FAILED --output json
)
json_expect("${integrity_expected_OUTPUT}" "expectation/matched" "ON")
json_expect("${integrity_expected_OUTPUT}" "result/current_execution_status" "INTEGRITY_FAILED")
json_expect("${integrity_expected_OUTPUT}" "result/exit_code" "5")

run_json(invalid_expectation 2 inspect --config "${config}"
         --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex" --record-root "${runs}"
         --expect-status INTERNAL_ERROR --output json)
json_expect("${invalid_expectation_OUTPUT}" "published_bundle" "")
run_json(missing_record_root 2 inspect --config "${config}"
         --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex" --output json)
run_json(replay_replacement 2 replay --bundle "${inspect_bundle}" --config "${config}"
         --record-root "${runs}" --output json)
run_json(compare_expectation 2 compare --left-run "${inspect_bundle}"
         --right-run "${replay_a_bundle}" --expect-status OK --output json)
run_json(cross_generation_compare 3 compare --left-run "${inspect_bundle}"
         --right-run "${PAE_SOURCE_DIR}/tests/protocol_lab/fixtures/legacy_offline_v0_1"
         --output json)

run_json(invalid_config 4 inspect --config "${PAE_FIXTURE_DIR}/invalid_config.pae.json"
         --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex" --record-root "${runs}" --output json)
json_expect("${invalid_config_OUTPUT}" "current_terminal_status" "PREPARATION_FAILED")
run_json(invalid_config_expected 4 inspect --config "${PAE_FIXTURE_DIR}/invalid_config.pae.json"
         --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex" --record-root "${runs}"
         --expect-status OK --output json)
json_expect("${invalid_config_expected_OUTPUT}" "expectation/matched" "OFF")

file(READ "${config}" unknown_schema_text)
string(REPLACE [["schema_version": "0.5"]] [["schema_version": "0.7"]]
               unknown_schema_text "${unknown_schema_text}")
set(unknown_schema_config "${PAE_BINARY_DIR}/unknown_schema.pae.json")
file(WRITE "${unknown_schema_config}" "${unknown_schema_text}")
run_json(unknown_schema 4 inspect --config "${unknown_schema_config}"
         --frame-hex "${PAE_FIXTURE_DIR}/valid.frame.hex" --record-root "${runs}" --output json)
json_expect("${unknown_schema_OUTPUT}" "current_terminal_status" "PREPARATION_FAILED")
json_expect("${unknown_schema_OUTPUT}" "diagnostic/id" "PAE_LAB_C1_CONFIG_INVALID")

run_json(invalid_values 5 encode --config "${config}"
         --values "${PAE_SOURCE_DIR}/tests/protocol_lab/fixtures/invalid_duplicate.values.pae-lab.json"
         --record-root "${runs}" --output json)
json_expect("${invalid_values_OUTPUT}" "current_terminal_status" "PREPARATION_FAILED")
json_expect("${invalid_values_OUTPUT}" "diagnostic/id" "PAE_LAB_C1_VALUES_INVALID")
json_expect("${invalid_values_OUTPUT}" "result" "")
string(JSON invalid_values_bundle GET "${invalid_values_OUTPUT}" published_bundle)
resolve_cli_path(invalid_values_bundle)
if(invalid_values_bundle STREQUAL "")
    message(FATAL_ERROR "Values preparation failure did not publish its complete failure Bundle")
endif()
file(GLOB before_no_result_replay_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH before_no_result_replay_runs before_no_result_replay_count)
run_json(no_result_replay 3 replay --bundle "${invalid_values_bundle}" --record-root "${runs}"
         --output json)
json_expect("${no_result_replay_OUTPUT}" "diagnostic/id" "PAE_LAB_C3_REPLAY_EVIDENCE_INVALID")
file(GLOB after_no_result_replay_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH after_no_result_replay_runs after_no_result_replay_count)
if(NOT before_no_result_replay_count EQUAL after_no_result_replay_count)
    message(FATAL_ERROR "no-Result Replay published a child Bundle")
endif()

run_json(missing_values_file 3 encode --config "${config}"
         --values "${PAE_BINARY_DIR}/does-not-exist.values.json" --record-root "${runs}"
         --output json)
json_expect("${missing_values_file_OUTPUT}" "diagnostic/id" "PAE_LAB_C3_VALUES_READ_FAILED")
json_expect("${missing_values_file_OUTPUT}" "published_bundle" "")

set(blocked_root "${PAE_BINARY_DIR}/record_root_is_file")
file(WRITE "${blocked_root}" "not a directory")
run_json(publish_failure 7 encode --config "${config}"
         --values "${PAE_FIXTURE_DIR}/valid.values.pae-lab.json"
         --record-root "${blocked_root}" --expect-status OK --output json)
json_expect("${publish_failure_OUTPUT}" "published_bundle" "")
json_expect("${publish_failure_OUTPUT}" "diagnostic/id" "PAE_LAB_C3_EVIDENCE_PUBLISH_FAILED")
json_expect("${publish_failure_OUTPUT}" "current_terminal_status" "OK")
json_expect("${publish_failure_OUTPUT}" "expectation/matched" "ON")
json_expect("${publish_failure_OUTPUT}" "result/format_version" "pae.lab.result/0.6")
json_expect("${publish_failure_OUTPUT}" "result/exit_code" "0")

file(COPY "${inspect_bundle}" DESTINATION "${PAE_BINARY_DIR}/corrupt")
get_filename_component(inspect_name "${inspect_bundle}" NAME)
set(corrupt "${PAE_BINARY_DIR}/corrupt/${inspect_name}")
file(APPEND "${corrupt}/result_summary_v0.6.json" " ")
file(GLOB before_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH before_runs before_count)
run_json(corrupt_replay 3 replay --bundle "${corrupt}" --record-root "${runs}" --output json)
file(GLOB after_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH after_runs after_count)
if(NOT before_count EQUAL after_count)
    message(FATAL_ERROR "corrupt replay published a child Bundle")
endif()

file(COPY "${inspect_bundle}" DESTINATION "${PAE_BINARY_DIR}/missing_record")
set(missing_record "${PAE_BINARY_DIR}/missing_record/${inspect_name}")
file(REMOVE "${missing_record}/run_record_v0.7.json")
file(GLOB before_missing_record_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH before_missing_record_runs before_missing_record_count)
run_json(missing_record_replay 3 replay --bundle "${missing_record}" --record-root "${runs}"
         --output json)
json_expect("${missing_record_replay_OUTPUT}" "diagnostic/id"
            "PAE_LAB_C3_REPLAY_EVIDENCE_INVALID")
file(GLOB after_missing_record_runs LIST_DIRECTORIES true "${runs}/*")
list(LENGTH after_missing_record_runs after_missing_record_count)
if(NOT before_missing_record_count EQUAL after_missing_record_count)
    message(FATAL_ERROR "missing-record replay published a child Bundle")
endif()

execute_process(
    COMMAND "${PAE_LAB_EXECUTABLE}" encode
            --config "tests/protocol_core/fixtures/decimal_core_contract.pae.json"
            --values "tests/protocol_lab_c3_cli/fixtures/valid.values.pae-lab.json"
            --record-root "${runs_cli}"
    WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
    RESULT_VARIABLE text_exit
    OUTPUT_VARIABLE text_output
    ERROR_VARIABLE text_stderr
)
if(NOT text_exit EQUAL 0 OR NOT text_output MATCHES "CLI_FORMAT=pae.lab.cli/0.1 COMMAND=encode EXIT_CODE=0" OR
   NOT text_output MATCHES [["format_version":"pae.lab.result/0.6"]])
    message(FATAL_ERROR "C3 text output contract failed: ${text_exit}\n${text_output}\n${text_stderr}")
endif()

execute_process(
    COMMAND "${PAE_LAB_EXECUTABLE}" inspect
            --config "examples/config/synthetic_int64_slice.pae.json"
            --frame-hex "tests/protocol_core/golden/synthetic_int64/int64_record_001.frame.hex"
            --output json
    WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
    RESULT_VARIABLE old_exit
    OUTPUT_VARIABLE old_output
    ERROR_VARIABLE old_stderr
)
if(NOT old_exit EQUAL 0)
    message(FATAL_ERROR "legacy Schema 0.4 path failed: ${old_exit}\n${old_output}\n${old_stderr}")
endif()
string(JSON old_format GET "${old_output}" format_version)
if(NOT old_format STREQUAL "pae.lab.result/0.5")
    message(FATAL_ERROR "legacy Schema 0.4 output changed: ${old_format}")
endif()

execute_process(COMMAND "${PAE_LAB_EXECUTABLE}" --help RESULT_VARIABLE help_exit)
execute_process(COMMAND "${PAE_LAB_EXECUTABLE}" --version RESULT_VARIABLE version_exit)
if(NOT help_exit EQUAL 0 OR NOT version_exit EQUAL 0)
    message(FATAL_ERROR "help/version compatibility failed")
endif()

message(STATUS "C3 CLI offline contract checks passed")
