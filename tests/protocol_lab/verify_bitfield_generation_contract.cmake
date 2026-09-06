if(NOT DEFINED PAE_LAB_EXECUTABLE OR NOT DEFINED PAE_LAB_DATA_DIR OR
   NOT DEFINED PAE_LAB_RUN_ROOT)
    message(FATAL_ERROR "DEC-040 Lab test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${PAE_LAB_RUN_ROOT}")
file(MAKE_DIRECTORY "${PAE_LAB_RUN_ROOT}")

function(run_lab expected_exit output_var)
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR "Lab exit mismatch: expected=${expected_exit} actual=${actual_exit}\n${output}\n${error_output}")
    endif()
    set(${output_var} "${output}" PARENT_SCOPE)
endfunction()

function(copy_mutate_v3_event source destination from to)
    file(REMOVE_RECURSE "${destination}")
    file(MAKE_DIRECTORY "${destination}")
    file(COPY "${source}/" DESTINATION "${destination}")
    set(event_file "${destination}/events_v0.3.jsonl")
    set(record_file "${destination}/run_record_v0.3.json")
    file(SHA256 "${event_file}" old_event_hash)
    file(SHA256 "${record_file}" old_record_hash)
    file(READ "${event_file}" event_text)
    string(FIND "${event_text}" "${from}" mutation_position)
    if(mutation_position EQUAL -1)
        message(FATAL_ERROR "Event mutation marker not found: ${from}")
    endif()
    string(REPLACE "${from}" "${to}" event_text "${event_text}")
    file(WRITE "${event_file}" "${event_text}")
    file(SHA256 "${event_file}" new_event_hash)
    file(READ "${record_file}" record_text)
    string(REPLACE "${old_event_hash}" "${new_event_hash}" record_text "${record_text}")
    file(WRITE "${record_file}" "${record_text}")
    file(SHA256 "${record_file}" new_record_hash)
    file(READ "${destination}/SHA256SUMS" sums)
    string(REPLACE "${old_event_hash}" "${new_event_hash}" sums "${sums}")
    string(REPLACE "${old_record_hash}" "${new_record_hash}" sums "${sums}")
    file(WRITE "${destination}/SHA256SUMS" "${sums}")
endfunction()

function(assert_json json path expected)
    string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${path})
    if(json_error OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "JSON assertion failed at ${path}: expected='${expected}' actual='${actual}' error='${json_error}'")
    endif()
endfunction()

function(assert_no_completed_bundle root)
    file(GLOB completed LIST_DIRECTORIES true "${root}/run_*")
    if(completed)
        message(FATAL_ERROR "Unexpected completed Evidence Bundle under ${root}: ${completed}")
    endif()
endfunction()

function(copy_mutate_v3_result source destination from to)
    file(REMOVE_RECURSE "${destination}")
    file(MAKE_DIRECTORY "${destination}")
    file(COPY "${source}/" DESTINATION "${destination}")
    set(result_file "${destination}/result_summary_v0.3.json")
    set(record_file "${destination}/run_record_v0.3.json")
    file(SHA256 "${result_file}" old_result_hash)
    file(SHA256 "${record_file}" old_record_hash)
    file(READ "${result_file}" result_text)
    string(FIND "${result_text}" "${from}" mutation_position)
    if(mutation_position EQUAL -1)
        message(FATAL_ERROR "Mutation marker not found: ${from}")
    endif()
    string(REPLACE "${from}" "${to}" result_text "${result_text}")
    file(WRITE "${result_file}" "${result_text}")
    file(SHA256 "${result_file}" new_result_hash)
    file(READ "${record_file}" record_text)
    string(REPLACE "${old_result_hash}" "${new_result_hash}" record_text "${record_text}")
    file(WRITE "${record_file}" "${record_text}")
    file(SHA256 "${record_file}" new_record_hash)
    file(READ "${destination}/SHA256SUMS" sums)
    string(REPLACE "${old_result_hash}" "${new_result_hash}" sums "${sums}")
    string(REPLACE "${old_record_hash}" "${new_record_hash}" sums "${sums}")
    file(WRITE "${destination}/SHA256SUMS" "${sums}")
endfunction()

set(config "${PAE_LAB_DATA_DIR}/synthetic_bitfield_slice.pae.json")
set(values "${PAE_LAB_DATA_DIR}/synthetic_bitfield_slice.values.pae-lab.json")
set(frame "${PAE_LAB_DATA_DIR}/bitfield_record_001.frame.hex")

# P2-1: once Schema 0.2 compiled, preparation failures stay in Result/Record/Event 0.3.
set(invalid_duplicate "${PAE_LAB_DATA_DIR}/invalid_duplicate.values.pae-lab.json")
set(v3_failure_root "${PAE_LAB_RUN_ROOT}/v3-values-failure")
run_lab(5 v3_values_failure encode --config "${config}" --values "${invalid_duplicate}"
        --record-root "${v3_failure_root}" --output json)
assert_json("${v3_values_failure}" format_version "pae.lab.result/0.3")
assert_json("${v3_values_failure}" operation_status "VALUES_INVALID")
assert_json("${v3_values_failure}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
assert_json("${v3_values_failure}" frame_hex "")
assert_json("${v3_values_failure}" replay_mode "ENCODE_TX")
assert_json("${v3_values_failure}" replay_subject "TX")
assert_json("${v3_values_failure}" current_execution_status "VALUES_INVALID")
assert_json("${v3_values_failure}" current_execution_diagnostic_id "PAE_LAB_VALUES_INVALID")
assert_json("${v3_values_failure}" comparison_status "NOT_APPLICABLE")
file(GLOB v3_failure_bundles LIST_DIRECTORIES true "${v3_failure_root}/run_*")
list(LENGTH v3_failure_bundles v3_failure_count)
if(NOT v3_failure_count EQUAL 1)
    message(FATAL_ERROR "Expected one readable V0.3 failure Bundle, got ${v3_failure_count}")
endif()
list(GET v3_failure_bundles 0 v3_failure_bundle)
run_lab(0 v3_failure_load compare --left-run "${v3_failure_bundle}"
        --right-run "${v3_failure_bundle}" --output json)
assert_json("${v3_failure_load}" operation_status "EQUAL")

set(v3_replay_failure_root "${PAE_LAB_RUN_ROOT}/v3-replay-failure")
run_lab(3 v3_replay_failure replay --bundle "${v3_failure_bundle}"
        --record-root "${v3_replay_failure_root}" --output json)
assert_json("${v3_replay_failure}" format_version "pae.lab.result/0.3")
assert_json("${v3_replay_failure}" operation_status "INPUT_ERROR")
assert_json("${v3_replay_failure}" "diagnostic;id" "PAE_LAB_REPLAY_VALUES_ERROR")
assert_json("${v3_replay_failure}" replay_mode "ENCODE_TX")
assert_json("${v3_replay_failure}" current_execution_status "INPUT_ERROR")
assert_json("${v3_replay_failure}" comparison_status "NOT_EVALUATED")
string(JSON replay_comparison_equal_type TYPE "${v3_replay_failure}" comparison_equal)
if(NOT replay_comparison_equal_type STREQUAL "NULL")
    message(FATAL_ERROR "Replay preparation failure must not claim a comparison result")
endif()
file(GLOB v3_replay_failure_bundles LIST_DIRECTORIES true
     "${v3_replay_failure_root}/run_*")
list(LENGTH v3_replay_failure_bundles v3_replay_failure_count)
if(NOT v3_replay_failure_count EQUAL 1)
    message(FATAL_ERROR "Expected one readable replay failure Bundle")
endif()
list(GET v3_replay_failure_bundles 0 v3_replay_failure_bundle)
run_lab(0 v3_replay_failure_load compare --left-run "${v3_replay_failure_bundle}"
        --right-run "${v3_replay_failure_bundle}" --output json)
assert_json("${v3_replay_failure_load}" operation_status "EQUAL")

set(missing_values_root "${PAE_LAB_RUN_ROOT}/missing-values")
run_lab(3 missing_values encode --config "${config}"
        --values "${PAE_LAB_RUN_ROOT}/does-not-exist.values.json"
        --record-root "${missing_values_root}" --output json)
assert_json("${missing_values}" format_version "pae.lab.result/0.3")
assert_json("${missing_values}" "diagnostic;id" "PAE_LAB_VALUES_INPUT_ERROR")
assert_json("${missing_values}" current_execution_status "INPUT_ERROR")
assert_no_completed_bundle("${missing_values_root}")

set(frame_input_root "${PAE_LAB_RUN_ROOT}/frame-input-error")
run_lab(3 frame_input inspect --config "${config}"
        --frame-hex "${PAE_LAB_DATA_DIR}/invalid_syntax.frame.hex"
        --record-root "${frame_input_root}" --output json)
assert_json("${frame_input}" format_version "pae.lab.result/0.3")
assert_json("${frame_input}" "diagnostic;id" "PAE_LAB_FRAME_INPUT_ERROR")
assert_json("${frame_input}" replay_mode "DECODE_RX")
assert_json("${frame_input}" current_execution_status "INPUT_ERROR")
assert_no_completed_bundle("${frame_input_root}")

set(oversized_frame "${PAE_LAB_RUN_ROOT}/oversized.frame.hex")
string(REPEAT "00" 19 oversized_frame_text)
file(WRITE "${oversized_frame}" "${oversized_frame_text}\n")
set(frame_limit_root "${PAE_LAB_RUN_ROOT}/frame-limit")
run_lab(3 frame_limit inspect --config "${config}" --frame-hex "${oversized_frame}"
        --record-root "${frame_limit_root}" --output json)
assert_json("${frame_limit}" format_version "pae.lab.result/0.3")
assert_json("${frame_limit}" "diagnostic;id" "PAE_LAB_FRAME_LIMIT_EXCEEDED")
assert_json("${frame_limit}" frame_length "19")
assert_json("${frame_limit}" replay_mode "DECODE_RX")
assert_json("${frame_limit}" current_execution_status "INPUT_ERROR")
file(GLOB frame_limit_bundles LIST_DIRECTORIES true "${frame_limit_root}/run_*")
list(GET frame_limit_bundles 0 frame_limit_bundle)
run_lab(0 frame_limit_load compare --left-run "${frame_limit_bundle}"
        --right-run "${frame_limit_bundle}" --output json)
assert_json("${frame_limit_load}" operation_status "EQUAL")

set(udp_pipeline_root "${PAE_LAB_RUN_ROOT}/udp-unknown-pipeline")
run_lab(2 udp_pipeline udp-exchange --config "${config}" --values "${values}"
        --local 127.0.0.1:0 --remote 127.0.0.1:9 --receive-pipeline missing_pipeline
        --send --record-root "${udp_pipeline_root}" --output json)
assert_json("${udp_pipeline}" format_version "pae.lab.result/0.3")
assert_json("${udp_pipeline}" "diagnostic;id" "PAE_LAB_UNKNOWN_RECEIVE_PIPELINE")
assert_json("${udp_pipeline}" current_execution_status "CLI_ERROR")
assert_json("${udp_pipeline}" send_attempted "OFF")
assert_no_completed_bundle("${udp_pipeline_root}")

set(udp_values_root "${PAE_LAB_RUN_ROOT}/udp-invalid-values")
run_lab(5 udp_values udp-exchange --config "${config}" --values "${invalid_duplicate}"
        --local 127.0.0.1:0 --remote 127.0.0.1:9
        --receive-pipeline synthetic_direction --send
        --record-root "${udp_values_root}" --output json)
assert_json("${udp_values}" format_version "pae.lab.result/0.3")
assert_json("${udp_values}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
assert_json("${udp_values}" current_execution_status "VALUES_INVALID")
assert_json("${udp_values}" send_attempted "OFF")
assert_no_completed_bundle("${udp_values_root}")

run_lab(0 original encode --config "${config}" --values "${values}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${original}" format_version "pae.lab.result/0.3")
assert_json("${original}" frame_hex "AD85A0434080BEEF01EFCDAB896745230154")
assert_json("${original}" "fields;0;kind" "BOOL")
assert_json("${original}" "fields;0;raw_value" "1")
assert_json("${original}" "fields;0;logical_value" "true")
assert_json("${original}" "fields;0;enum_known" "OFF")
assert_json("${original}" "fields;7;kind" "BOOL")
assert_json("${original}" "fields;7;raw_value" "0")
assert_json("${original}" "fields;7;logical_value" "false")
assert_json("${original}" "fields;7;enum_known" "OFF")
file(GLOB bundles LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(SORT bundles)
list(GET bundles -1 original_bundle)

run_lab(0 replay_a replay --bundle "${original_bundle}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${replay_a}" format_version "pae.lab.result/0.3")
assert_json("${replay_a}" replay_mode "ENCODE_TX")
assert_json("${replay_a}" replay_subject "TX")
assert_json("${replay_a}" current_execution_status "OK")
assert_json("${replay_a}" comparison_status "EQUAL")
assert_json("${replay_a}" comparison_equal "ON")
file(GLOB bundles LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(SORT bundles)
list(GET bundles -1 replay_a_bundle)

run_lab(0 replay_b replay --bundle "${replay_a_bundle}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${replay_b}" format_version "pae.lab.result/0.3")
assert_json("${replay_b}" replay_mode "ENCODE_TX")
assert_json("${replay_b}" replay_subject "TX")
assert_json("${replay_b}" current_execution_status "OK")
assert_json("${replay_b}" comparison_status "EQUAL")
assert_json("${replay_b}" comparison_equal "ON")

# C10: same-generation replacement configuration may produce DIFFERENT or a readable failure Run.
file(READ "${config}" changed_config_text)
string(REPLACE "\"base_value\": 160" "\"base_value\": 176"
       changed_config_text "${changed_config_text}")
set(changed_config "${PAE_LAB_RUN_ROOT}/changed-config.pae.json")
file(WRITE "${changed_config}" "${changed_config_text}")
run_lab(6 changed_replay replay --bundle "${original_bundle}" --config "${changed_config}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${changed_replay}" comparison_status "DIFFERENT")
assert_json("${changed_replay}" cross_config_replay "ON")
assert_json("${changed_replay}" frame_hex "BD85A0434080BEEF01EFCDAB896745230154")
file(GLOB bundles LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(SORT bundles)
list(GET bundles -1 changed_bundle)
run_lab(0 changed_chain replay --bundle "${changed_bundle}" --output json)
assert_json("${changed_chain}" comparison_status "EQUAL")

file(READ "${config}" failing_config_text)
string(REPLACE "\"id\": \"active\"" "\"id\": \"active_removed\""
       failing_config_text "${failing_config_text}")
set(failing_config "${PAE_LAB_RUN_ROOT}/failing-config.pae.json")
file(WRITE "${failing_config}" "${failing_config_text}")
run_lab(5 failing_replay replay --bundle "${original_bundle}" --config "${failing_config}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${failing_replay}" current_execution_status "VALUES_INVALID")
assert_json("${failing_replay}" "diagnostic;id" "PAE_LAB_VALUES_UNKNOWN_ENUM_ENTRY")
assert_json("${failing_replay}" frame_hex "")
assert_json("${failing_replay}" comparison_status "DIFFERENT")
assert_json("${failing_replay}" comparison_equal "OFF")
file(GLOB bundles LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(SORT bundles)
list(GET bundles -1 failing_bundle)
run_lab(5 failing_chain replay --bundle "${failing_bundle}" --output json)
assert_json("${failing_chain}" current_execution_status "VALUES_INVALID")
assert_json("${failing_chain}" comparison_status "EQUAL")
assert_json("${failing_chain}" comparison_equal "ON")

run_lab(0 raw_equal compare --left-frame-hex "${frame}" --right-frame-hex "${frame}" --output json)
assert_json("${raw_equal}" operation_status "EQUAL")
assert_json("${raw_equal}" comparison_equal "ON")

set(old_config "${PAE_LAB_DATA_DIR}/synthetic_lab_exchange_slice.pae.json")
set(old_values "${PAE_LAB_DATA_DIR}/valid_command.values.pae-lab.json")
run_lab(0 old_run encode --config "${old_config}" --values "${old_values}"
        --record-root "${PAE_LAB_RUN_ROOT}" --output json)
assert_json("${old_run}" format_version "pae.lab.result/0.1")

set(old_failure_root "${PAE_LAB_RUN_ROOT}/v1-values-failure")
run_lab(5 old_failure encode --config "${old_config}" --values "${invalid_duplicate}"
        --record-root "${old_failure_root}" --output json)
assert_json("${old_failure}" format_version "pae.lab.result/0.1")
assert_json("${old_failure}" operation_status "VALUES_INVALID")
assert_json("${old_failure}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
file(GLOB bundles LIST_DIRECTORIES true "${PAE_LAB_RUN_ROOT}/run_*")
list(SORT bundles)
list(GET bundles -1 old_bundle)

# C03: Schema 0.2 without BOOL may consume Values 0.1, but output is still 0.3.
file(READ "${old_config}" schema02_no_bool_text)
string(REPLACE "\"schema_version\": \"0.1\"" "\"schema_version\": \"0.2\""
       schema02_no_bool_text "${schema02_no_bool_text}")
set(schema02_no_bool "${PAE_LAB_RUN_ROOT}/schema02-no-bool.pae.json")
file(WRITE "${schema02_no_bool}" "${schema02_no_bool_text}")
run_lab(0 schema02_old_values encode --config "${schema02_no_bool}" --values "${old_values}" --output json)
assert_json("${schema02_old_values}" format_version "pae.lab.result/0.3")

# C04: Schema 0.1 may consume Values 0.2 when it contains only legacy kinds.
file(READ "${old_values}" values02_old_types_text)
string(REPLACE "pae.lab.values/0.1" "pae.lab.values/0.2"
       values02_old_types_text "${values02_old_types_text}")
set(values02_old_types "${PAE_LAB_RUN_ROOT}/values02-old-types.json")
file(WRITE "${values02_old_types}" "${values02_old_types_text}")
run_lab(0 schema01_new_values encode --config "${old_config}" --values "${values02_old_types}" --output json)
assert_json("${schema01_new_values}" format_version "pae.lab.result/0.1")

# C02/C06: the old Schema and Values versions do not gain BOOL/bitfield semantics.
file(READ "${config}" schema01_bitfield_text)
string(REPLACE "\"schema_version\": \"0.2\"" "\"schema_version\": \"0.1\""
       schema01_bitfield_text "${schema01_bitfield_text}")
set(schema01_bitfield "${PAE_LAB_RUN_ROOT}/schema01-bitfield.pae.json")
file(WRITE "${schema01_bitfield}" "${schema01_bitfield_text}")
run_lab(4 schema01_bitfield_result encode --config "${schema01_bitfield}" --values "${values}" --output json)
assert_json("${schema01_bitfield_result}" "diagnostic;id" "PAE_LAB_CONFIG_COMPILE_FAILED")

file(READ "${values}" values01_bool_text)
string(REPLACE "pae.lab.values/0.2" "pae.lab.values/0.1" values01_bool_text "${values01_bool_text}")
set(values01_bool "${PAE_LAB_RUN_ROOT}/values01-bool.json")
file(WRITE "${values01_bool}" "${values01_bool_text}")
run_lab(5 values01_bool_result encode --config "${config}" --values "${values01_bool}" --output json)
assert_json("${values01_bool_result}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
assert_json("${values01_bool_result}" format_version "pae.lab.result/0.3")
assert_json("${values01_bool_result}" current_execution_status "VALUES_INVALID")

run_lab(3 cross_replay replay --bundle "${old_bundle}" --config "${config}" --output json)
assert_json("${cross_replay}" "diagnostic;id" "PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED")
assert_json("${cross_replay}" format_version "pae.lab.result/0.3")
assert_json("${cross_replay}" comparison_status "NOT_EVALUATED")
run_lab(3 reverse_cross_replay replay --bundle "${original_bundle}" --config "${old_config}" --output json)
assert_json("${reverse_cross_replay}" "diagnostic;id" "PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED")
assert_json("${reverse_cross_replay}" format_version "pae.lab.result/0.1")
assert_json("${reverse_cross_replay}" operation_status "INPUT_ERROR")
run_lab(3 cross_compare compare --left-run "${old_bundle}" --right-run "${original_bundle}" --output json)
assert_json("${cross_compare}" "diagnostic;id" "PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED")
assert_json("${cross_compare}" operation_status "INPUT_ERROR")
run_lab(3 reverse_cross_compare compare --left-run "${original_bundle}" --right-run "${old_bundle}" --output json)
assert_json("${reverse_cross_compare}" "diagnostic;id" "PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED")
assert_json("${reverse_cross_compare}" operation_status "INPUT_ERROR")

# C07/C14: self-consistent hashes do not excuse invalid BOOL tuples or unknown generations.
set(bool_tampered "${PAE_LAB_RUN_ROOT}/tampered-bool")
copy_mutate_v3_result("${original_bundle}" "${bool_tampered}"
                      "\"raw_value\":\"1\",\"logical_value\":\"true\""
                      "\"raw_value\":\"0\",\"logical_value\":\"true\"")
run_lab(3 bool_tampered_result replay --bundle "${bool_tampered}" --output json)
assert_json("${bool_tampered_result}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")

set(bool_enum_tampered "${PAE_LAB_RUN_ROOT}/tampered-bool-enum-known")
copy_mutate_v3_result("${original_bundle}" "${bool_enum_tampered}"
                      "\"kind\":\"BOOL\",\"raw_value\":\"1\",\"logical_value\":\"true\",\"enum_known\":false"
                      "\"kind\":\"BOOL\",\"raw_value\":\"1\",\"logical_value\":\"true\",\"enum_known\":true")
run_lab(3 bool_enum_tampered_result replay --bundle "${bool_enum_tampered}" --output json)
assert_json("${bool_enum_tampered_result}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")

set(version_tampered "${PAE_LAB_RUN_ROOT}/tampered-version")
copy_mutate_v3_result("${original_bundle}" "${version_tampered}"
                      "pae.lab.result/0.3" "pae.lab.result/9.9")
run_lab(3 version_tampered_result replay --bundle "${version_tampered}" --output json)
assert_json("${version_tampered_result}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")

set(mixed_event "${PAE_LAB_RUN_ROOT}/tampered-mixed-event")
copy_mutate_v3_event("${original_bundle}" "${mixed_event}"
                     "pae.lab.event/0.3" "pae.lab.event/0.2")
run_lab(3 mixed_event_result replay --bundle "${mixed_event}" --output json)
assert_json("${mixed_event_result}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")

set(invalid_bool "${PAE_LAB_RUN_ROOT}/invalid-bool.values.json")
file(READ "${values}" invalid_bool_text)
string(REPLACE "\"bool\": true" "\"bool\": 1" invalid_bool_text "${invalid_bool_text}")
file(WRITE "${invalid_bool}" "${invalid_bool_text}")
run_lab(5 invalid_bool_result encode --config "${config}" --values "${invalid_bool}" --output json)
assert_json("${invalid_bool_result}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
assert_json("${invalid_bool_result}" format_version "pae.lab.result/0.3")
assert_json("${invalid_bool_result}" current_execution_status "VALUES_INVALID")

foreach(invalid_literal IN ITEMS 0 "\"true\"")
    file(READ "${values}" invalid_literal_text)
    string(REPLACE "\"bool\": true" "\"bool\": ${invalid_literal}"
           invalid_literal_text "${invalid_literal_text}")
    string(MAKE_C_IDENTIFIER "${invalid_literal}" invalid_suffix)
    set(invalid_path "${PAE_LAB_RUN_ROOT}/invalid-bool-${invalid_suffix}.values.json")
    file(WRITE "${invalid_path}" "${invalid_literal_text}")
    run_lab(5 invalid_result encode --config "${config}" --values "${invalid_path}" --output json)
    assert_json("${invalid_result}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
    assert_json("${invalid_result}" format_version "pae.lab.result/0.3")
    assert_json("${invalid_result}" current_execution_status "VALUES_INVALID")
endforeach()

message(STATUS "PAE_DEC040_LAB_GENERATION_PASS")
