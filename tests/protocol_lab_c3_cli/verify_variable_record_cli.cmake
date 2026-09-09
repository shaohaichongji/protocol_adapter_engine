cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS PAE_LAB_EXECUTABLE PAE_SOURCE_DIR PAE_BINARY_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(config "${PAE_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json")
set(values "${PAE_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.values.pae-lab.json")
set(root "${PAE_BINARY_DIR}/variable-record-runs")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")
set(bad_length "${root}/bad-length.hex")
file(WRITE "${bad_length}" "A5 04 10 20 D9\n")
set(bad_sum "${root}/bad-sum.hex")
file(WRITE "${bad_sum}" "A5 05 10 20 DB\n")
set(empty_frame "${root}/empty.frame.hex")
file(WRITE "${empty_frame}" "A5 03 A8\n")
set(empty_values "${root}/empty.values.json")
file(WRITE "${empty_values}" [=[
{"format_version":"pae.lab.values/0.5","pipeline_id":"synthetic_rx","message_id":"bounded_record","fields":[{"id":"payload","kind":"BYTES","hex":""}]}
]=])
set(legacy_empty_values "${root}/legacy-empty.values.json")
file(WRITE "${legacy_empty_values}" [=[
{"format_version":"pae.lab.values/0.4","pipeline_id":"synthetic_rx","message_id":"bounded_record","fields":[{"id":"payload","kind":"BYTES","hex":""}]}
]=])
set(null_empty_values "${root}/null-empty.values.json")
file(WRITE "${null_empty_values}" [=[
{"format_version":"pae.lab.values/0.5","pipeline_id":"synthetic_rx","message_id":"bounded_record","fields":[{"id":"payload","kind":"BYTES","hex":null}]}
]=])
set(missing_empty_values "${root}/missing-empty.values.json")
file(WRITE "${missing_empty_values}" [=[
{"format_version":"pae.lab.values/0.5","pipeline_id":"synthetic_rx","message_id":"bounded_record","fields":[{"id":"payload","kind":"BYTES"}]}
]=])
file(READ "${config}" minimum_one_config_text)
string(REPLACE "\"min_payload_bytes\": 0" "\"min_payload_bytes\": 1"
       minimum_one_config_text "${minimum_one_config_text}")
set(minimum_one_config "${root}/minimum-one.pae.json")
file(WRITE "${minimum_one_config}" "${minimum_one_config_text}")
set(v07_values_v05 "${root}/v07.values-0.5.json")
file(WRITE "${v07_values_v05}" [=[
{"format_version":"pae.lab.values/0.5","pipeline_id":"synthetic_rx","message_id":"frame_length_record","fields":[{"id":"value","kind":"UINT64","uint64":"5"},{"id":"payload","kind":"BYTES","hex":"7E"}]}
]=])
set(decimal_config "${root}/bounded-decimal.pae.json")
file(WRITE "${decimal_config}" [=[
{"schema_version":"0.8","protocol_id":"synthetic_bounded_decimal","protocol_version":"1","display_name":"Synthetic bounded Decimal","description":"Public non-identity Decimal and bounded payload vector.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","resource_profile":"desktop","framing_profiles":[{"id":"record","display_name":"Record","description":"Complete record.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","input_kind":"complete_record"}],"pipelines":[{"id":"pipe","display_name":"Pipe","description":"Offline pipeline.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","direction_id":"rx","input_framing_profile_id":"record","message_ids":["message"]}],"messages":[{"id":"message","display_name":"Message","description":"Four-byte header, bounded payload, and SUM8 trailer.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","direction_id":"rx","layout":{"kind":"bounded_payload","header_length_bytes":4,"payload_field_id":"payload","min_payload_bytes":0,"max_payload_bytes":2},"matcher":{"all":[{"kind":"fixed_bytes","byte_offset":0,"bytes":"A5"}]},"integrity":{"algorithm":"sum8","range":{"byte_offset":0,"end":"payload_end"},"storage":{"anchor":"payload_end"}},"fields":[{"id":"length","display_name":"Length","description":"Actual frame length.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":1,"byte_width":1},"encode":{"source":"computed"},"computed":{"kind":"length","scope":"frame"}},{"id":"scaled","display_name":"Scaled","description":"Logical equals raw times two plus one.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":2,"byte_width":2,"byte_order":"big_endian"},"encode":{"source":"input"},"conversion":{"kind":"linear","output_type":"DECIMAL64","scale":{"numerator":2,"denominator":1},"bias":{"numerator":1,"denominator":1}}},{"id":"payload","display_name":"Payload","description":"Bounded bytes.","source_ref":"SYNTHETIC_FROM_SCRATCH:bounded_decimal","value_type":"BYTES","wire":{"codec":"bytes","byte_offset":4},"encode":{"source":"input"}}]}]}
]=])
set(decimal_payload_values "${root}/bounded-decimal-payload.values.json")
file(WRITE "${decimal_payload_values}" [=[
{"format_version":"pae.lab.values/0.4","pipeline_id":"pipe","message_id":"message","fields":[{"id":"scaled","kind":"DECIMAL64","decimal64":{"coefficient":"7","scale":0}},{"id":"payload","kind":"BYTES","hex":"1020"}]}
]=])

function(run_json name expected_exit)
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
        RESULT_VARIABLE actual_exit OUTPUT_VARIABLE output ERROR_VARIABLE stderr
    )
    file(WRITE "${root}/${name}.json" "${output}")
    file(WRITE "${root}/${name}.stderr.txt" "${stderr}")
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: expected ${expected_exit}, got ${actual_exit}\n${output}\n${stderr}")
    endif()
    string(JSON declared_exit GET "${output}" process_exit_code)
    if(NOT declared_exit EQUAL expected_exit)
        message(FATAL_ERROR "${name}: process_exit_code mismatch")
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

function(resolve_bundle json variable)
    string(JSON path GET "${json}" published_bundle)
    if(IS_ABSOLUTE "${path}")
        set(${variable} "${path}" PARENT_SCOPE)
    else()
        set(${variable} "${PAE_SOURCE_DIR}/${path}" PARENT_SCOPE)
    endif()
endfunction()

function(encode_string value output)
    string(LENGTH "${value}" length)
    set(${output} "S${length}:${value}" PARENT_SCOPE)
endfunction()

function(encode_optional json key output)
    string(JSON type TYPE "${json}" "${key}")
    if(type STREQUAL "NULL")
        set(encoded "N;")
    else()
        string(JSON value GET "${json}" "${key}")
        encode_string("${value}" encoded)
    endif()
    set(${output} "${encoded}" PARENT_SCOPE)
endfunction()

function(variable_fingerprint json output)
    set(payload "A20:S23:pae.lab.fingerprint/0.9S3:0.8")
    foreach(key IN ITEMS operation_kind replay_mode replay_subject current_execution_status
                         current_execution_diagnostic_id config_sha256 protocol_id pipeline_id
                         message_id direction_id frame_hex tx_frame_hex rx_frame_hex
                         conversion_error failed_field_id)
        encode_optional("${json}" "${key}" encoded)
        string(APPEND payload "${encoded}")
    endforeach()
    foreach(key IN ITEMS failed_field_index failed_value_index)
        string(JSON type TYPE "${json}" "${key}")
        if(type STREQUAL "NULL")
            string(APPEND payload "N;")
        else()
            string(JSON value GET "${json}" "${key}")
            string(LENGTH "${value}" length)
            string(APPEND payload "I${length}:${value}")
        endif()
    endforeach()
    string(JSON field_count LENGTH "${json}" fields)
    string(APPEND payload "A${field_count}:")
    math(EXPR last "${field_count} - 1")
    foreach(index RANGE 0 ${last})
        string(APPEND payload "A5:")
        foreach(key IN ITEMS id kind raw_value logical_value)
            string(JSON value GET "${json}" fields ${index} ${key})
            encode_string("${value}" encoded)
            string(APPEND payload "${encoded}")
        endforeach()
        string(JSON enum_known GET "${json}" fields ${index} enum_known)
        if(enum_known)
            string(APPEND payload "B1;")
        else()
            string(APPEND payload "B0;")
        endif()
    endforeach()
    string(SHA256 fingerprint "${payload}")
    string(TOUPPER "${fingerprint}" fingerprint)
    set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()

function(assert_semantic_tamper_rejected source)
    get_filename_component(name "${source}" NAME)
    set(case_root "${root}/semantic-tamper")
    set(bundle "${case_root}/${name}")
    file(MAKE_DIRECTORY "${bundle}")
    file(COPY "${source}/" DESTINATION "${bundle}")
    set(result_file "${bundle}/result_summary_v0.9.json")
    set(record_file "${bundle}/run_record_v0.10.json")
    file(READ "${result_file}" result)
    string(JSON result SET "${result}" fields 1 raw_value "\"10\"")
    string(JSON result SET "${result}" fields 1 logical_value "\"10\"")
    variable_fingerprint("${result}" fingerprint)
    string(JSON result SET "${result}" deterministic_fingerprint "\"${fingerprint}\"")
    file(WRITE "${result_file}" "${result}")
    file(SIZE "${result_file}" result_size)
    file(SHA256 "${result_file}" result_hash)

    file(READ "${record_file}" record)
    string(JSON record SET "${record}" deterministic_fingerprint "\"${fingerprint}\"")
    string(JSON count LENGTH "${record}" recorded_payload_files)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${record}" recorded_payload_files ${index} path)
        if(path STREQUAL "result_summary_v0.9.json")
            string(JSON record SET "${record}" recorded_payload_files ${index} size "${result_size}")
            string(JSON record SET "${record}" recorded_payload_files ${index} sha256 "\"${result_hash}\"")
        endif()
    endforeach()
    file(WRITE "${record_file}" "${record}")

    file(STRINGS "${bundle}/SHA256SUMS" lines)
    set(manifest "")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^[0-9a-f]+  (.+)$")
            message(FATAL_ERROR "malformed manifest")
        endif()
        set(path "${CMAKE_MATCH_1}")
        file(SHA256 "${bundle}/${path}" hash)
        string(APPEND manifest "${hash}  ${path}\n")
    endforeach()
    file(WRITE "${bundle}/SHA256SUMS" "${manifest}")

    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" compare --left-run "${bundle}" --right-run "${bundle}"
                --output json
        WORKING_DIRECTORY "${PAE_SOURCE_DIR}"
        RESULT_VARIABLE exit OUTPUT_VARIABLE output ERROR_VARIABLE stderr
    )
    file(WRITE "${case_root}/rejection.json" "${output}")
    if(NOT exit EQUAL 3)
        message(FATAL_ERROR "semantic tamper expected exit 3, got ${exit}\n${output}\n${stderr}")
    endif()
    json_expect("${output}" "diagnostic/id" "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID")
    json_expect("${output}" "diagnostic/detail"
                "successful bounded Result payload does not bind its actual Frame layout")
endfunction()

run_json(encode 0 encode --config "${config}" --values "${values}"
         --record-root "${root}" --output json)
json_expect("${encode_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${encode_OUTPUT}" "result/frame_hex" "A5051020DA")
json_expect("${encode_OUTPUT}" "result/fields/0/raw_value" "5")
json_expect("${encode_OUTPUT}" "result/fields/1/raw_value" "1020")
json_expect("${encode_OUTPUT}" "result/current_execution_status" "OK")
resolve_bundle("${encode_OUTPUT}" original)
if(NOT EXISTS "${original}/run_record_v0.10.json" OR
   NOT EXISTS "${original}/result_summary_v0.9.json" OR
   NOT EXISTS "${original}/events_v0.7.jsonl")
    message(FATAL_ERROR "Schema 0.8 generation files are incomplete")
endif()
file(SHA256 "${original}/run_record_v0.10.json" original_record_hash)
file(SHA256 "${original}/result_summary_v0.9.json" original_result_hash)
assert_semantic_tamper_rejected("${original}")

run_json(compare 0 compare --left-run "${original}" --right-run "${original}" --output json)
json_expect("${compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(replay_a 0 replay --bundle "${original}" --record-root "${root}/a" --output json)
json_expect("${replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${replay_a_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
resolve_bundle("${replay_a_OUTPUT}" replay_a_bundle)
run_json(replay_b 0 replay --bundle "${replay_a_bundle}" --record-root "${root}/b" --output json)
json_expect("${replay_b_OUTPUT}" "comparison/status" "EQUAL")
file(SHA256 "${original}/run_record_v0.10.json" replayed_record_hash)
file(SHA256 "${original}/result_summary_v0.9.json" replayed_result_hash)
if(NOT original_record_hash STREQUAL replayed_record_hash OR
   NOT original_result_hash STREQUAL replayed_result_hash)
    message(FATAL_ERROR "Replay modified the original Schema 0.8 Bundle")
endif()

run_json(empty_encode 0 encode --config "${config}" --values "${empty_values}"
         --record-root "${root}/empty-encode" --output json)
json_expect("${empty_encode_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${empty_encode_OUTPUT}" "result/frame_hex" "A503A8")
json_expect("${empty_encode_OUTPUT}" "result/fields/0/raw_value" "3")
json_expect("${empty_encode_OUTPUT}" "result/fields/1/kind" "BYTES")
json_expect("${empty_encode_OUTPUT}" "result/fields/1/raw_value" "")
json_expect("${empty_encode_OUTPUT}" "result/fields/1/logical_value" "")
resolve_bundle("${empty_encode_OUTPUT}" empty_encode_bundle)
run_json(empty_encode_compare 0 compare --left-run "${empty_encode_bundle}"
         --right-run "${empty_encode_bundle}" --output json)
json_expect("${empty_encode_compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(empty_encode_replay_a 0 replay --bundle "${empty_encode_bundle}"
         --record-root "${root}/empty-encode-a" --output json)
json_expect("${empty_encode_replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${empty_encode_replay_a_OUTPUT}" "result/frame_hex" "A503A8")
resolve_bundle("${empty_encode_replay_a_OUTPUT}" empty_encode_replay_a_bundle)
run_json(empty_encode_replay_b 0 replay --bundle "${empty_encode_replay_a_bundle}"
         --record-root "${root}/empty-encode-b" --output json)
json_expect("${empty_encode_replay_b_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${empty_encode_replay_b_OUTPUT}" "result/fields/1/raw_value" "")

run_json(empty_inspect 0 inspect --config "${config}" --frame-hex "${empty_frame}"
         --record-root "${root}/empty-inspect" --output json)
json_expect("${empty_inspect_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${empty_inspect_OUTPUT}" "result/frame_hex" "A503A8")
json_expect("${empty_inspect_OUTPUT}" "result/fields/1/kind" "BYTES")
json_expect("${empty_inspect_OUTPUT}" "result/fields/1/raw_value" "")
resolve_bundle("${empty_inspect_OUTPUT}" empty_inspect_bundle)
run_json(empty_inspect_compare 0 compare --left-run "${empty_inspect_bundle}"
         --right-run "${empty_inspect_bundle}" --output json)
json_expect("${empty_inspect_compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(empty_inspect_replay 0 replay --bundle "${empty_inspect_bundle}"
         --record-root "${root}/empty-inspect-a" --output json)
json_expect("${empty_inspect_replay_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${empty_inspect_replay_OUTPUT}" "result/fields/1/raw_value" "")

run_json(minimum_one_empty 5 encode --config "${minimum_one_config}" --values "${empty_values}"
         --record-root "${root}/minimum-one" --output json)
json_expect("${minimum_one_empty_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${minimum_one_empty_OUTPUT}" "result/current_execution_status"
            "BYTES_LENGTH_MISMATCH")
json_expect("${minimum_one_empty_OUTPUT}" "result/failed_field_id" "payload")
json_expect("${minimum_one_empty_OUTPUT}" "result/failed_value_index" "0")
resolve_bundle("${minimum_one_empty_OUTPUT}" minimum_one_bundle)
run_json(minimum_one_compare 0 compare --left-run "${minimum_one_bundle}"
         --right-run "${minimum_one_bundle}" --output json)
json_expect("${minimum_one_compare_OUTPUT}" "comparison/status" "EQUAL")
run_json(minimum_one_replay 5 replay --bundle "${minimum_one_bundle}"
         --record-root "${root}/minimum-one-a" --output json)
json_expect("${minimum_one_replay_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${minimum_one_replay_OUTPUT}" "result/current_execution_status"
            "BYTES_LENGTH_MISMATCH")

foreach(invalid_case IN ITEMS legacy_empty null_empty missing_empty)
    run_json(${invalid_case} 5 encode --config "${config}" --values "${${invalid_case}_values}"
             --record-root "${root}/${invalid_case}" --output json)
    json_expect("${${invalid_case}_OUTPUT}" "diagnostic/id" "PAE_LAB_C1_VALUES_INVALID")
    json_expect("${${invalid_case}_OUTPUT}" "current_terminal_status" "PREPARATION_FAILED")
endforeach()
run_json(v07_rejects_v05 5 encode
         --config "${PAE_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json"
         --values "${v07_values_v05}" --record-root "${root}/v07-v05" --output json)
json_expect("${v07_rejects_v05_OUTPUT}" "diagnostic/id" "PAE_LAB_C1_VALUES_INVALID")
json_expect("${v07_rejects_v05_OUTPUT}" "diagnostic/detail"
            "Values 0.5 is accepted only for Schema 0.8 execution")
json_expect("${v07_rejects_v05_OUTPUT}" "current_terminal_status" "PREPARATION_FAILED")

run_json(decimal_payload 0 encode --config "${decimal_config}"
         --values "${decimal_payload_values}" --record-root "${root}/decimal-payload"
         --output json)
json_expect("${decimal_payload_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${decimal_payload_OUTPUT}" "result/frame_hex" "A50700031020DF")
json_expect("${decimal_payload_OUTPUT}" "result/fields/1/kind" "DECIMAL64")
json_expect("${decimal_payload_OUTPUT}" "result/fields/1/raw_value" "3")
json_expect("${decimal_payload_OUTPUT}" "result/fields/1/decimal64/coefficient" "7")
json_expect("${decimal_payload_OUTPUT}" "result/fields/2/raw_value" "1020")
resolve_bundle("${decimal_payload_OUTPUT}" decimal_original)
file(SHA256 "${decimal_original}/run_record_v0.10.json" decimal_record_hash)
file(SHA256 "${decimal_original}/result_summary_v0.9.json" decimal_result_hash)
run_json(decimal_replay_a 0 replay --bundle "${decimal_original}"
         --record-root "${root}/decimal-a" --output json)
json_expect("${decimal_replay_a_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${decimal_replay_a_OUTPUT}" "result/frame_hex" "A50700031020DF")
json_expect("${decimal_replay_a_OUTPUT}" "result/fields/1/raw_value" "3")
resolve_bundle("${decimal_replay_a_OUTPUT}" decimal_replay_a_bundle)
run_json(decimal_replay_b 0 replay --bundle "${decimal_replay_a_bundle}"
         --record-root "${root}/decimal-b" --output json)
json_expect("${decimal_replay_b_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${decimal_replay_b_OUTPUT}" "result/frame_hex" "A50700031020DF")
file(SHA256 "${decimal_original}/run_record_v0.10.json" decimal_replayed_record_hash)
file(SHA256 "${decimal_original}/result_summary_v0.9.json" decimal_replayed_result_hash)
if(NOT decimal_record_hash STREQUAL decimal_replayed_record_hash OR
   NOT decimal_result_hash STREQUAL decimal_replayed_result_hash)
    message(FATAL_ERROR "Decimal Replay modified the original Schema 0.8 Bundle")
endif()

run_json(length_failure 5 inspect --config "${config}" --frame-hex "${bad_length}"
         --record-root "${root}/length" --output json)
json_expect("${length_failure_OUTPUT}" "result/format_version" "pae.lab.result/0.9")
json_expect("${length_failure_OUTPUT}" "result/current_execution_status" "LENGTH_MISMATCH")
json_expect("${length_failure_OUTPUT}" "result/failed_field_id" "record_length")
json_expect("${length_failure_OUTPUT}" "result/fields" "[]")
resolve_bundle("${length_failure_OUTPUT}" length_bundle)
run_json(length_replay 5 replay --bundle "${length_bundle}" --record-root "${root}/length-a"
         --output json)
json_expect("${length_replay_OUTPUT}" "comparison/status" "EQUAL")
json_expect("${length_replay_OUTPUT}" "result/current_execution_status" "LENGTH_MISMATCH")

run_json(sum_failure 5 inspect --config "${config}" --frame-hex "${bad_sum}"
         --record-root "${root}/sum" --output json)
json_expect("${sum_failure_OUTPUT}" "result/current_execution_status" "INTEGRITY_FAILED")
json_expect("${sum_failure_OUTPUT}" "result/fields" "[]")

message(STATUS "Schema 0.8 bounded variable Lab checks passed")
