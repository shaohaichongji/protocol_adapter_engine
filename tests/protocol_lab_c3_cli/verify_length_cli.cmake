cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS PAE_LAB_EXECUTABLE PAE_SOURCE_DIR PAE_BINARY_DIR PAE_FIXTURE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(config "${PAE_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json")
set(values "${PAE_FIXTURE_DIR}/length.values.pae-lab.json")
set(root "${PAE_BINARY_DIR}/length-runs")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")
set(bad_frame "${root}/bad-length.frame.hex")
file(WRITE "${bad_frame}" "AA 00 05 05 7E 55\n")
set(override_values "${root}/override.values.json")
file(WRITE "${override_values}"
     "{\"format_version\":\"pae.lab.values/0.1\",\"pipeline_id\":\"synthetic_rx\","
     "\"message_id\":\"frame_length_record\",\"fields\":["
     "{\"id\":\"value\",\"kind\":\"UINT64\",\"uint64\":\"5\"},"
     "{\"id\":\"record_length\",\"kind\":\"UINT64\",\"uint64\":\"6\"}]}\n")
set(override_wrong_type_values "${root}/override-wrong-type.values.json")
file(WRITE "${override_wrong_type_values}"
     "{\"format_version\":\"pae.lab.values/0.1\",\"pipeline_id\":\"synthetic_rx\","
     "\"message_id\":\"frame_length_record\",\"fields\":["
     "{\"id\":\"record_length\",\"kind\":\"BYTES\",\"hex\":\"00\"}]}\n")

function(run_json name expected_exit)
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE output
        ERROR_VARIABLE stderr
    )
    file(WRITE "${root}/${name}.json" "${output}")
    file(WRITE "${root}/${name}.stderr.txt" "${stderr}")
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR
                "${name}: expected exit ${expected_exit}, got ${actual_exit}\n${output}\n${stderr}")
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

function(resolve_only_bundle directory variable)
    file(GLOB candidates LIST_DIRECTORIES TRUE "${directory}/run_*")
    list(LENGTH candidates count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${directory}: expected exactly one published Run, found ${count}")
    endif()
    list(GET candidates 0 path)
    set(${variable} "${path}" PARENT_SCOPE)
endfunction()

function(encode_fingerprint_value json key output)
    string(JSON value_type TYPE "${json}" "${key}")
    if(value_type STREQUAL "NULL")
        set(encoded "N;")
    else()
        string(JSON value GET "${json}" "${key}")
        string(LENGTH "${value}" value_length)
        set(encoded "S${value_length}:${value}")
    endif()
    set(${output} "${encoded}" PARENT_SCOPE)
endfunction()

function(encode_fingerprint_index json key output)
    string(JSON value_type TYPE "${json}" "${key}")
    if(value_type STREQUAL "NULL")
        set(encoded "N;")
    else()
        string(JSON value GET "${json}" "${key}")
        string(LENGTH "${value}" value_length)
        set(encoded "I${value_length}:${value}")
    endif()
    set(${output} "${encoded}" PARENT_SCOPE)
endfunction()

function(length_result_fingerprint json output)
    string(JSON field_count LENGTH "${json}" fields)
    if(NOT field_count EQUAL 0)
        message(FATAL_ERROR "length Result tamper helper only supports failed empty-field Results")
    endif()
    set(payload "A20:S23:pae.lab.fingerprint/0.8S3:0.7")
    foreach(key IN ITEMS operation_kind replay_mode replay_subject current_execution_status
                         current_execution_diagnostic_id config_sha256 protocol_id pipeline_id
                         message_id direction_id frame_hex tx_frame_hex rx_frame_hex
                         conversion_error failed_field_id)
        encode_fingerprint_value("${json}" "${key}" encoded)
        string(APPEND payload "${encoded}")
    endforeach()
    foreach(key IN ITEMS failed_field_index failed_value_index)
        encode_fingerprint_index("${json}" "${key}" encoded)
        string(APPEND payload "${encoded}")
    endforeach()
    string(APPEND payload "A0:")
    string(SHA256 fingerprint "${payload}")
    string(TOUPPER "${fingerprint}" fingerprint)
    set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()

function(assert_tampered_override_rejected source case_name replacement expected_detail)
    get_filename_component(bundle_name "${source}" NAME)
    set(case_root "${root}/tampered-${case_name}")
    set(bundle "${case_root}/${bundle_name}")
    file(REMOVE_RECURSE "${case_root}")
    file(MAKE_DIRECTORY "${bundle}")
    file(COPY "${source}/" DESTINATION "${bundle}")
    set(result_file "${bundle}/result_summary_v0.8.json")
    set(record_file "${bundle}/run_record_v0.9.json")
    file(READ "${result_file}" result_text)
    string(JSON result_text SET "${result_text}" failed_value_index "${replacement}")
    length_result_fingerprint("${result_text}" fingerprint)
    string(JSON result_text SET "${result_text}" deterministic_fingerprint "\"${fingerprint}\"")
    file(WRITE "${result_file}" "${result_text}")
    file(SIZE "${result_file}" result_size)
    file(SHA256 "${result_file}" result_hash)

    file(READ "${record_file}" record_text)
    string(JSON record_text SET "${record_text}" deterministic_fingerprint "\"${fingerprint}\"")
    string(JSON payload_count LENGTH "${record_text}" recorded_payload_files)
    math(EXPR payload_last "${payload_count} - 1")
    set(result_bound OFF)
    foreach(index RANGE 0 ${payload_last})
        string(JSON payload_path GET "${record_text}" recorded_payload_files ${index} path)
        if(payload_path STREQUAL "result_summary_v0.8.json")
            string(JSON record_text SET "${record_text}" recorded_payload_files ${index} size
                   "${result_size}")
            string(JSON record_text SET "${record_text}" recorded_payload_files ${index} sha256
                   "\"${result_hash}\"")
            set(result_bound ON)
        endif()
    endforeach()
    if(NOT result_bound)
        message(FATAL_ERROR "${case_name}: Result payload entry is missing")
    endif()
    file(WRITE "${record_file}" "${record_text}")

    file(STRINGS "${bundle}/SHA256SUMS" manifest_lines)
    set(new_manifest "")
    foreach(line IN LISTS manifest_lines)
        if(NOT line MATCHES "^[0-9a-f]+  (.+)$")
            message(FATAL_ERROR "${case_name}: malformed manifest line")
        endif()
        set(relative_path "${CMAKE_MATCH_1}")
        file(SHA256 "${bundle}/${relative_path}" actual_hash)
        string(APPEND new_manifest "${actual_hash}  ${relative_path}\n")
    endforeach()
    file(WRITE "${bundle}/SHA256SUMS" "${new_manifest}")

    file(READ "${record_file}" verified_record)
    string(JSON payload_count LENGTH "${verified_record}" recorded_payload_files)
    math(EXPR payload_last "${payload_count} - 1")
    foreach(index RANGE 0 ${payload_last})
        string(JSON payload_path GET "${verified_record}" recorded_payload_files ${index} path)
        string(JSON payload_size GET "${verified_record}" recorded_payload_files ${index} size)
        string(JSON payload_hash GET "${verified_record}" recorded_payload_files ${index} sha256)
        file(SIZE "${bundle}/${payload_path}" actual_size)
        file(SHA256 "${bundle}/${payload_path}" actual_hash)
        if(NOT actual_size EQUAL payload_size OR NOT actual_hash STREQUAL payload_hash)
            message(FATAL_ERROR "${case_name}: Record payload association is inconsistent")
        endif()
    endforeach()
    file(STRINGS "${bundle}/SHA256SUMS" verified_manifest)
    foreach(line IN LISTS verified_manifest)
        if(NOT line MATCHES "^([0-9a-f]+)  (.+)$")
            message(FATAL_ERROR "${case_name}: malformed verified manifest line")
        endif()
        file(SHA256 "${bundle}/${CMAKE_MATCH_2}" actual_hash)
        if(NOT actual_hash STREQUAL CMAKE_MATCH_1)
            message(FATAL_ERROR "${case_name}: manifest association is inconsistent")
        endif()
    endforeach()

    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" compare --left-run "${bundle}" --right-run "${bundle}"
                --output json
        WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE output
        ERROR_VARIABLE stderr
    )
    file(WRITE "${case_root}/rejection.json" "${output}")
    if(NOT actual_exit EQUAL 3)
        message(FATAL_ERROR
                "${case_name}: expected semantic rejection exit 3, got ${actual_exit}\n${output}\n${stderr}")
    endif()
    json_expect("${output}" "diagnostic/id" "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")
    json_expect("${output}" "diagnostic/detail" "${expected_detail}")
endfunction()

run_json(length_encode 0 encode --config "${config}" --values "${values}"
         --record-root "${root}/success" --output json)
json_expect("${length_encode_OUTPUT}" "result/format_version" "pae.lab.result/0.8")
json_expect("${length_encode_OUTPUT}" "result/frame_hex" "AA0006057E55")
json_expect("${length_encode_OUTPUT}" "result/fields/0/id" "record_length")
json_expect("${length_encode_OUTPUT}" "result/fields/0/raw_value" "6")
json_expect("${length_encode_OUTPUT}" "result/replay_mode" "ENCODE_TX")
json_expect("${length_encode_OUTPUT}" "result/replay_subject" "TX")
json_expect("${length_encode_OUTPUT}" "result/current_execution_status" "OK")
string(JSON encode_fingerprint GET "${length_encode_OUTPUT}" result deterministic_fingerprint)
resolve_only_bundle("${root}/success" encode_bundle)
if(NOT EXISTS "${encode_bundle}/run_record_v0.9.json" OR
   NOT EXISTS "${encode_bundle}/result_summary_v0.8.json" OR
   NOT EXISTS "${encode_bundle}/events_v0.7.jsonl")
    message(FATAL_ERROR "Schema 0.7 Run did not publish Record 0.9, Result 0.8, and Event 0.7")
endif()
file(SHA256 "${encode_bundle}/run_record_v0.9.json" original_record_hash)
file(SHA256 "${encode_bundle}/result_summary_v0.8.json" original_result_hash)

run_json(encode_self_compare 0 compare --left-run "${encode_bundle}"
         --right-run "${encode_bundle}" --output json)
json_expect("${encode_self_compare_OUTPUT}" "comparison/status" "EQUAL")

run_json(replay_a 0 replay --bundle "${encode_bundle}" --record-root "${root}/replay-a"
         --output json)
json_expect("${replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${replay_a_OUTPUT}" "result/format_version" "pae.lab.result/0.8")
json_expect("${replay_a_OUTPUT}" "result/replay_mode" "ENCODE_TX")
json_expect("${replay_a_OUTPUT}" "result/replay_subject" "TX")
json_expect("${replay_a_OUTPUT}" "result/current_execution_status" "OK")
json_expect("${replay_a_OUTPUT}" "result/deterministic_fingerprint" "${encode_fingerprint}")
resolve_only_bundle("${root}/replay-a" replay_a_bundle)
run_json(replay_b 0 replay --bundle "${replay_a_bundle}" --record-root "${root}/replay-b"
         --output json)
json_expect("${replay_b_OUTPUT}" "comparison/status" "EQUAL")
file(SHA256 "${encode_bundle}/run_record_v0.9.json" replayed_record_hash)
file(SHA256 "${encode_bundle}/result_summary_v0.8.json" replayed_result_hash)
if(NOT replayed_record_hash STREQUAL original_record_hash OR
   NOT replayed_result_hash STREQUAL original_result_hash)
    message(FATAL_ERROR "Replay modified the original Schema 0.7 Bundle")
endif()

run_json(length_failure 5 inspect --config "${config}" --frame-hex "${bad_frame}"
         --record-root "${root}/failure" --output json)
json_expect("${length_failure_OUTPUT}" "diagnostic/id" "PAE_LAB_CODEC_LENGTH_MISMATCH")
json_expect("${length_failure_OUTPUT}" "result/current_execution_status" "LENGTH_MISMATCH")
json_expect("${length_failure_OUTPUT}" "result/failed_field_id" "record_length")
json_expect("${length_failure_OUTPUT}" "result/failed_field_index" "0")
json_expect("${length_failure_OUTPUT}" "result/fields" "[]")
json_expect("${length_failure_OUTPUT}" "result/frame_hex" "AA0005057E55")
json_expect("${length_failure_OUTPUT}" "result/replay_mode" "DECODE_RX")
json_expect("${length_failure_OUTPUT}" "result/replay_subject" "RX")
resolve_only_bundle("${root}/failure" failure_bundle)
run_json(failure_replay_a 5 replay --bundle "${failure_bundle}"
         --record-root "${root}/failure-replay-a" --output json)
json_expect("${failure_replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${failure_replay_a_OUTPUT}" "result/current_execution_status" "LENGTH_MISMATCH")
json_expect("${failure_replay_a_OUTPUT}" "result/failed_field_id" "record_length")
json_expect("${failure_replay_a_OUTPUT}" "result/fields" "[]")
resolve_only_bundle("${root}/failure-replay-a" failure_replay_a_bundle)
run_json(failure_replay_b 5 replay --bundle "${failure_replay_a_bundle}"
         --record-root "${root}/failure-replay-b" --output json)
json_expect("${failure_replay_b_OUTPUT}" "comparison/status" "EQUAL")

run_json(override 5 encode --config "${config}" --values "${override_values}"
         --record-root "${root}/override" --output json)
json_expect("${override_OUTPUT}" "diagnostic/id" "PAE_LAB_CODEC_COMPUTED_FIELD_OVERRIDE")
json_expect("${override_OUTPUT}" "result/current_execution_status" "COMPUTED_FIELD_OVERRIDE")
json_expect("${override_OUTPUT}" "result/failed_value_index" "1")
json_expect("${override_OUTPUT}" "result/failed_field_id" "record_length")
json_expect("${override_OUTPUT}" "result/frame_hex" "")
resolve_only_bundle("${root}/override" override_bundle)
run_json(override_self_compare 0 compare --left-run "${override_bundle}"
         --right-run "${override_bundle}" --output json)
json_expect("${override_self_compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(override_replay 5 replay --bundle "${override_bundle}"
         --record-root "${root}/override-replay" --output json)
json_expect("${override_replay_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${override_replay_OUTPUT}" "result/current_execution_status"
            "COMPUTED_FIELD_OVERRIDE")
json_expect("${override_replay_OUTPUT}" "result/failed_value_index" "1")

assert_tampered_override_rejected(
    "${override_bundle}" missing-index null
    "computed field override does not bind the Message computed length field and Values input")
assert_tampered_override_rejected(
    "${override_bundle}" outside-index 99
    "failed Values index is outside the recorded author order")
assert_tampered_override_rejected(
    "${override_bundle}" wrong-field 0
    "failed Values index does not bind the failed field identity")

run_json(override_wrong_type 5 encode --config "${config}" --values "${override_wrong_type_values}"
         --record-root "${root}/override-wrong-type" --output json)
json_expect("${override_wrong_type_OUTPUT}" "diagnostic/id"
            "PAE_LAB_CODEC_COMPUTED_FIELD_OVERRIDE")
json_expect("${override_wrong_type_OUTPUT}" "result/failed_value_index" "0")
resolve_only_bundle("${root}/override-wrong-type" override_wrong_type_bundle)
run_json(override_wrong_type_compare 0 compare --left-run "${override_wrong_type_bundle}"
         --right-run "${override_wrong_type_bundle}" --output json)
json_expect("${override_wrong_type_compare_OUTPUT}" "comparison/status" "EQUAL")

set(no_length_config "${root}/schema-v07-no-length.pae.json")
set(no_length_values "${PAE_FIXTURE_DIR}/schema_v06_no_integrity.values.pae-lab.json")
file(READ "${PAE_FIXTURE_DIR}/schema_v06_no_integrity.pae.json" no_length_text)
string(REPLACE [["schema_version": "0.6"]] [["schema_version": "0.7"]]
       no_length_text "${no_length_text}")
file(WRITE "${no_length_config}" "${no_length_text}")
run_json(no_length 0 encode --config "${no_length_config}" --values "${no_length_values}"
         --record-root "${root}/no-length" --output json)
json_expect("${no_length_OUTPUT}" "result/format_version" "pae.lab.result/0.8")
resolve_only_bundle("${root}/no-length" no_length_bundle)
if(NOT EXISTS "${no_length_bundle}/run_record_v0.9.json" OR
   NOT EXISTS "${no_length_bundle}/result_summary_v0.8.json")
    message(FATAL_ERROR "Schema 0.7 without computed length did not use the new generation")
endif()

set(old_config "${PAE_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json")
set(old_values "${PAE_FIXTURE_DIR}/valid.values.pae-lab.json")
run_json(old_encode 0 encode --config "${old_config}" --values "${old_values}"
         --record-root "${root}/old" --output json)
resolve_only_bundle("${root}/old" old_bundle)
run_json(cross_generation 3 compare --left-run "${old_bundle}" --right-run "${encode_bundle}"
         --output json)
json_expect("${cross_generation_OUTPUT}" "diagnostic/id"
            "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")

message(STATUS "PAE fixed-record computed length Lab 0.8/0.9 CLI contract passed")
