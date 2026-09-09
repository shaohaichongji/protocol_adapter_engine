cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS PAE_LAB_EXECUTABLE PAE_SOURCE_DIR PAE_BINARY_DIR PAE_FIXTURE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(config "${PAE_SOURCE_DIR}/examples/config/synthetic_crc_slice.pae.json")
set(root "${PAE_BINARY_DIR}/crc-runs")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")
set(values "${root}/crc.values.json")
set(bad_frame "${root}/bad.frame.hex")
file(WRITE "${values}"
     "{\"format_version\":\"pae.lab.values/0.1\",\"pipeline_id\":\"synthetic_rx\","
     "\"message_id\":\"crc_record\",\"fields\":[{\"id\":\"payload\",\"kind\":\"BYTES\","
     "\"hex\":\"313233343536373839\"}]}\n")
file(WRITE "${bad_frame}" "30323334353637383929B1AA\n")

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
    file(WRITE "${root}/${name}.json" "${output}")
    file(WRITE "${root}/${name}.stderr.txt" "${stderr}")
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: expected exit ${expected_exit}, got ${actual_exit}\n${output}\n${stderr}")
    endif()
    string(JSON envelope_version GET "${output}" format_version)
    string(JSON declared_exit GET "${output}" process_exit_code)
    if(NOT envelope_version STREQUAL "pae.lab.cli/0.1" OR
       NOT declared_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: CLI envelope is inconsistent")
    endif()
    set(${name}_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

function(json_expect json path expected)
    string(REPLACE "/" ";" parts "${path}")
    string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${parts})
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${path}: expected '${expected}', got '${actual}' (${json_error})")
    endif()
endfunction()

function(resolve_bundle output variable)
    string(JSON path GET "${output}" published_bundle)
    if(NOT IS_ABSOLUTE "${path}")
        get_filename_component(path "${path}" ABSOLUTE BASE_DIR "${PAE_SOURCE_DIR}")
    endif()
    set(${variable} "${path}" PARENT_SCOPE)
endfunction()

run_json(crc_encode 0 encode --config "${config}" --values "${values}"
         --record-root "${root}/success" --output json)
json_expect("${crc_encode_OUTPUT}" "result/format_version" "pae.lab.result/0.7")
json_expect("${crc_encode_OUTPUT}" "result/frame_hex" "31323334353637383929B1AA")
json_expect("${crc_encode_OUTPUT}" "result/current_execution_status" "OK")
resolve_bundle("${crc_encode_OUTPUT}" encode_bundle)
if(NOT EXISTS "${encode_bundle}/run_record_v0.8.json" OR
   NOT EXISTS "${encode_bundle}/result_summary_v0.7.json")
    message(FATAL_ERROR "Schema 0.6 Run did not publish Record 0.8 and Result 0.7")
endif()

run_json(replay_a 0 replay --bundle "${encode_bundle}" --record-root "${root}/replay-a"
         --output json)
json_expect("${replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${replay_a_OUTPUT}" "result/format_version" "pae.lab.result/0.7")
resolve_bundle("${replay_a_OUTPUT}" replay_a_bundle)
run_json(replay_b 0 replay --bundle "${replay_a_bundle}" --record-root "${root}/replay-b"
         --output json)
json_expect("${replay_b_OUTPUT}" "comparison/status" "EQUAL")

run_json(crc_failure 5 inspect --config "${config}" --frame-hex "${bad_frame}"
         --record-root "${root}/failure" --output json)
json_expect("${crc_failure_OUTPUT}" "diagnostic/id" "PAE_LAB_CODEC_INTEGRITY_FAILED")
json_expect("${crc_failure_OUTPUT}" "result/current_execution_status" "INTEGRITY_FAILED")
json_expect("${crc_failure_OUTPUT}" "result/fields" "[]")
resolve_bundle("${crc_failure_OUTPUT}" failure_bundle)
run_json(failure_replay 5 replay --bundle "${failure_bundle}"
         --record-root "${root}/failure-replay" --output json)
json_expect("${failure_replay_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${failure_replay_OUTPUT}" "result/current_execution_status" "INTEGRITY_FAILED")

set(no_integrity_config "${PAE_FIXTURE_DIR}/schema_v06_no_integrity.pae.json")
set(no_integrity_values "${PAE_FIXTURE_DIR}/schema_v06_no_integrity.values.pae-lab.json")
run_json(no_integrity_encode 0 encode --config "${no_integrity_config}"
         --values "${no_integrity_values}" --record-root "${root}/no-integrity" --output json)
json_expect("${no_integrity_encode_OUTPUT}" "result/format_version" "pae.lab.result/0.7")
json_expect("${no_integrity_encode_OUTPUT}" "result/frame_hex" "31AA")

set(sum8_config "${root}/schema-v06-sum8.pae.json")
file(READ "${PAE_SOURCE_DIR}/examples/config/synthetic_sum8_slice.pae.json" sum8_text)
string(REPLACE [["schema_version": "0.3"]] [["schema_version": "0.6"]]
       sum8_text "${sum8_text}")
file(WRITE "${sum8_config}" "${sum8_text}")
run_json(sum8_encode 0 encode --config "${sum8_config}"
         --values "${PAE_SOURCE_DIR}/examples/config/synthetic_sum8_slice.values.pae-lab.json"
         --record-root "${root}/sum8" --output json)
json_expect("${sum8_encode_OUTPUT}" "result/format_version" "pae.lab.result/0.7")

set(old_config "${PAE_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json")
set(old_values "${PAE_FIXTURE_DIR}/valid.values.pae-lab.json")
run_json(old_encode 0 encode --config "${old_config}" --values "${old_values}"
         --record-root "${root}/old" --output json)
resolve_bundle("${old_encode_OUTPUT}" old_bundle)
run_json(old_self_compare 0 compare --left-run "${old_bundle}" --right-run "${old_bundle}"
         --output json)
json_expect("${old_self_compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(cross_generation 3 compare --left-run "${old_bundle}" --right-run "${encode_bundle}"
         --output json)
json_expect("${cross_generation_OUTPUT}" "diagnostic/id"
            "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")

if(DEFINED PAE_HISTORY_GENERATION_FIXTURE)
    set(history_root "${root}/history-generation")
    execute_process(
        COMMAND "${PAE_HISTORY_GENERATION_FIXTURE}" --prepare-history-generation "${history_root}"
        RESULT_VARIABLE history_fixture_exit
        OUTPUT_VARIABLE history_fixture_output
        ERROR_VARIABLE history_fixture_stderr
    )
    file(WRITE "${root}/history-generation-fixture.stdout.txt" "${history_fixture_output}")
    file(WRITE "${root}/history-generation-fixture.stderr.txt" "${history_fixture_stderr}")
    if(NOT history_fixture_exit EQUAL 0)
        message(FATAL_ERROR
                "history-generation fixture failed: ${history_fixture_exit}\n"
                "${history_fixture_output}\n${history_fixture_stderr}")
    endif()

    set(old_mixed "${history_root}/run_history_old_mixed")
    set(crc_mixed "${history_root}/run_history_crc_mixed")
    run_json(history_old_to_crc 3 compare --left-run "${old_mixed}" --right-run "${old_mixed}"
             --output json)
    json_expect("${history_old_to_crc_OUTPUT}" "diagnostic/id"
                "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")
    json_expect("${history_old_to_crc_OUTPUT}" "diagnostic/detail"
                "Replay historical Result generation does not match the parent Run Record")
    run_json(history_crc_to_old 3 compare --left-run "${crc_mixed}" --right-run "${crc_mixed}"
             --output json)
    json_expect("${history_crc_to_old_OUTPUT}" "diagnostic/id"
                "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")
    json_expect("${history_crc_to_old_OUTPUT}" "diagnostic/detail"
                "Replay historical Result generation does not match the parent Run Record")
endif()

message(STATUS "PAE CRC Lab 0.7/0.8 CLI contract passed")
