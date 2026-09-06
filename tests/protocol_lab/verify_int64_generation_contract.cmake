if(NOT DEFINED PAE_LAB_EXECUTABLE OR NOT DEFINED PAE_LAB_DATA_DIR OR
   NOT DEFINED PAE_LAB_RUN_ROOT)
    message(FATAL_ERROR "DEC-042A Lab test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${PAE_LAB_RUN_ROOT}")
file(MAKE_DIRECTORY "${PAE_LAB_RUN_ROOT}")

function(run_lab expected_exit output_var)
    execute_process(COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN}
                    RESULT_VARIABLE actual_exit OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT actual_exit EQUAL expected_exit)
        message(FATAL_ERROR "Lab exit mismatch expected=${expected_exit} actual=${actual_exit}\n${output}\n${errors}")
    endif()
    set(${output_var} "${output}" PARENT_SCOPE)
endfunction()

function(assert_json json path expected)
    string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${path})
    if(json_error OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "JSON ${path}: expected='${expected}' actual='${actual}' error='${json_error}'")
    endif()
endfunction()

function(only_bundle root output)
    file(GLOB bundles LIST_DIRECTORIES true "${root}/run_*")
    list(LENGTH bundles count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Expected one Bundle under ${root}, got ${count}")
    endif()
    list(GET bundles 0 selected)
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()

set(config "${PAE_LAB_DATA_DIR}/synthetic_int64_slice.pae.json")
set(values "${PAE_LAB_DATA_DIR}/synthetic_int64_slice.values.pae-lab.json")
set(frame "${PAE_LAB_DATA_DIR}/int64_record_001.frame.hex")

set(original_root "${PAE_LAB_RUN_ROOT}/original")
run_lab(0 encoded encode --config "${config}" --values "${values}" --record-root
        "${original_root}" --output json)
assert_json("${encoded}" format_version "pae.lab.result/0.5")
assert_json("${encoded}" operation_status "OK")
assert_json("${encoded}" frame_hex "FFFFFE000080FFFFFFFFFE8000000000000000FFA1CAFE0260")
assert_json("${encoded}" "fields;0;kind" "INT64")
assert_json("${encoded}" "fields;0;raw_value" "-2")
assert_json("${encoded}" "fields;3;raw_value" "-9223372036854775808")
only_bundle("${original_root}" original)

run_lab(0 inspected inspect --config "${config}" --frame-hex "${frame}" --output json)
assert_json("${inspected}" format_version "pae.lab.result/0.5")
assert_json("${inspected}" "fields;1;raw_value" "-8388608")

set(replay_a_root "${PAE_LAB_RUN_ROOT}/replay-a")
run_lab(0 replay_a replay --bundle "${original}" --record-root "${replay_a_root}" --output json)
assert_json("${replay_a}" comparison_status "EQUAL")
assert_json("${replay_a}" comparison_equal "ON")
assert_json("${replay_a}" current_execution_status "OK")
only_bundle("${replay_a_root}" replay_a_bundle)
set(replay_b_root "${PAE_LAB_RUN_ROOT}/replay-b")
run_lab(0 replay_b replay --bundle "${replay_a_bundle}" --record-root "${replay_b_root}" --output json)
assert_json("${replay_b}" format_version "pae.lab.result/0.5")
assert_json("${replay_b}" comparison_status "EQUAL")

# Same-generation replacement configuration changes actual Encode bytes, and the DIFFERENT Bundle
# remains readable and replayable against the execution checkpoint it records.
file(READ "${config}" cross_config_text)
string(REPLACE "\"value\":-2" "\"value\":-3" cross_config_text "${cross_config_text}")
set(cross_config "${PAE_LAB_RUN_ROOT}/cross.pae.json")
file(WRITE "${cross_config}" "${cross_config_text}")
set(cross_root "${PAE_LAB_RUN_ROOT}/cross")
run_lab(6 cross replay --bundle "${original}" --config "${cross_config}" --record-root
        "${cross_root}" --output json)
assert_json("${cross}" comparison_status "DIFFERENT")
assert_json("${cross}" cross_config_replay "ON")
assert_json("${cross}" current_execution_status "OK")
only_bundle("${cross_root}" cross_bundle)
set(cross_again_root "${PAE_LAB_RUN_ROOT}/cross-again")
run_lab(0 cross_again replay --bundle "${cross_bundle}" --record-root
        "${cross_again_root}" --output json)
assert_json("${cross_again}" comparison_status "EQUAL")

file(READ "${values}" values_text)
string(REPLACE "\"int64\":\"-2\"" "\"int64\":\"-0\"" bad_values "${values_text}")
file(WRITE "${PAE_LAB_RUN_ROOT}/negative-zero.values.json" "${bad_values}")
run_lab(5 bad encode --config "${config}" --values
        "${PAE_LAB_RUN_ROOT}/negative-zero.values.json" --output json)
assert_json("${bad}" format_version "pae.lab.result/0.5")
assert_json("${bad}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")

function(assert_invalid_int64_value case_name replacement)
    string(REPLACE "\"int64\":\"-2\"" "${replacement}" invalid_values "${values_text}")
    set(invalid_path "${PAE_LAB_RUN_ROOT}/${case_name}.values.json")
    file(WRITE "${invalid_path}" "${invalid_values}")
    run_lab(5 invalid_result encode --config "${config}" --values "${invalid_path}" --output json)
    assert_json("${invalid_result}" format_version "pae.lab.result/0.5")
    assert_json("${invalid_result}" "diagnostic;id" "PAE_LAB_VALUES_INVALID")
endfunction()
assert_invalid_int64_value(plus-one [["int64":"+1"]])
assert_invalid_int64_value(leading-zero [["int64":"01"]])
assert_invalid_int64_value(surrounding-space [["int64":" -2"]])
assert_invalid_int64_value(json-number [["int64":-2]])
assert_invalid_int64_value(positive-int64-overflow [["int64":"9223372036854775808"]])
assert_invalid_int64_value(negative-int64-overflow [["int64":"-9223372036854775809"]])

string(REPLACE "\"int64\":\"-2\"" "\"int64\":\"8388608\"" overflow_values
       "${values_text}")
set(failure_values "${PAE_LAB_RUN_ROOT}/overflow.values.json")
file(WRITE "${failure_values}" "${overflow_values}")
set(failure_root "${PAE_LAB_RUN_ROOT}/failure")
run_lab(5 failure encode --config "${config}" --values "${failure_values}" --record-root
        "${failure_root}" --output json)
assert_json("${failure}" format_version "pae.lab.result/0.5")
assert_json("${failure}" operation_status "VALUE_NOT_REPRESENTABLE")
assert_json("${failure}" frame_hex "")
only_bundle("${failure_root}" failure_bundle)
set(failure_replay_root "${PAE_LAB_RUN_ROOT}/failure-replay")
run_lab(5 failure_replay replay --bundle "${failure_bundle}" --record-root
        "${failure_replay_root}" --output json)
assert_json("${failure_replay}" comparison_status "EQUAL")
assert_json("${failure_replay}" current_execution_status "VALUE_NOT_REPRESENTABLE")

# Values 0.3 is generation-scoped even when it contains only legacy field kinds.
file(READ "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.pae.json" legacy_config_text)
file(READ "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.values.pae-lab.json" legacy_values_text)
string(REGEX REPLACE "pae.lab.values/0.[12]" "pae.lab.values/0.3" legacy_v3_values
       "${legacy_values_text}")
file(WRITE "${PAE_LAB_RUN_ROOT}/legacy-v3.values.json" "${legacy_v3_values}")
run_lab(5 values_generation encode --config "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.pae.json"
        --values "${PAE_LAB_RUN_ROOT}/legacy-v3.values.json" --output json)
assert_json("${values_generation}" "diagnostic;id" "PAE_LAB_VALUES_VERSION_MISMATCH")

# Schema 0.4 selects the 0.5 evidence generation even if the message has no INT64.
string(REPLACE "\"schema_version\": \"0.3\"" "\"schema_version\": \"0.4\""
       schema4_without_int "${legacy_config_text}")
file(WRITE "${PAE_LAB_RUN_ROOT}/schema4-without-int.pae.json" "${schema4_without_int}")
set(no_int_root "${PAE_LAB_RUN_ROOT}/schema4-without-int")
run_lab(0 no_int encode --config "${PAE_LAB_RUN_ROOT}/schema4-without-int.pae.json"
        --values "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.values.pae-lab.json"
        --record-root "${no_int_root}" --output json)
assert_json("${no_int}" format_version "pae.lab.result/0.5")
only_bundle("${no_int_root}" no_int_bundle)

# A Run from the preceding Schema generation is valid on its own but cannot be directly compared
# with the new 0.5 fingerprint domain.
set(schema3_root "${PAE_LAB_RUN_ROOT}/schema3")
run_lab(0 schema3 encode --config "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.pae.json"
        --values "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.values.pae-lab.json"
        --record-root "${schema3_root}" --output json)
only_bundle("${schema3_root}" schema3_bundle)
run_lab(3 cross_generation compare --left-run "${schema3_bundle}" --right-run
        "${no_int_bundle}" --output json)
assert_json("${cross_generation}" "diagnostic;id" "PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED")

file(READ "${config}" old_config)
string(REPLACE "\"schema_version\": \"0.4\"" "\"schema_version\": \"0.3\""
       old_config "${old_config}")
file(WRITE "${PAE_LAB_RUN_ROOT}/old-schema.pae.json" "${old_config}")
run_lab(4 old_result encode --config "${PAE_LAB_RUN_ROOT}/old-schema.pae.json"
        --values "${values}" --output json)
assert_json("${old_result}" "diagnostic;id" "PAE_LAB_CONFIG_COMPILE_FAILED")

# Semantic tamper: update target bytes, Run Record payload size/hash, and SHA256SUMS before read.
function(assert_semantic_tamper case_name old_text new_text expected_detail)
set(tampered "${PAE_LAB_RUN_ROOT}/tampered-${case_name}")
file(COPY "${original}/" DESTINATION "${tampered}")
set(result_file "${tampered}/result_summary_v0.5.json")
set(record_file "${tampered}/run_record_v0.5.json")
file(SIZE "${result_file}" old_size)
file(SHA256 "${result_file}" old_hash)
file(SHA256 "${record_file}" old_record_hash)
file(READ "${result_file}" result_text)
string(FIND "${result_text}" "${old_text}" mutation_offset)
if(mutation_offset EQUAL -1)
    message(FATAL_ERROR "${case_name}: mutation target missing")
endif()
string(REPLACE "${old_text}" "${new_text}" result_text "${result_text}")
file(WRITE "${result_file}" "${result_text}")
file(SIZE "${result_file}" new_size)
file(SHA256 "${result_file}" new_hash)
file(READ "${record_file}" record_text)
string(REPLACE
       "{\"path\":\"result_summary_v0.5.json\",\"size\":${old_size},\"sha256\":\"${old_hash}\"}"
       "{\"path\":\"result_summary_v0.5.json\",\"size\":${new_size},\"sha256\":\"${new_hash}\"}"
       record_text "${record_text}")
file(WRITE "${record_file}" "${record_text}")
file(SHA256 "${record_file}" new_record_hash)
file(READ "${tampered}/SHA256SUMS" sums)
string(REPLACE "${old_hash}" "${new_hash}" sums "${sums}")
string(REPLACE "${old_record_hash}" "${new_record_hash}" sums "${sums}")
file(WRITE "${tampered}/SHA256SUMS" "${sums}")
file(SIZE "${result_file}" verified_size)
file(SHA256 "${result_file}" verified_hash)
if(NOT verified_size EQUAL new_size OR NOT verified_hash STREQUAL new_hash)
    message(FATAL_ERROR "tampered V0.5 bundle is not self-consistent before semantic read")
endif()
# Verify every persisted Record payload entry and every checksum manifest entry, not just the
# altered file's newly calculated values. Only a self-consistent bundle may exercise the Reader.
file(READ "${record_file}" verified_record)
string(JSON payload_count LENGTH "${verified_record}" recorded_payload_files)
math(EXPR payload_last "${payload_count} - 1")
foreach(index RANGE 0 ${payload_last})
    string(JSON payload_path GET "${verified_record}" recorded_payload_files ${index} path)
    string(JSON payload_size GET "${verified_record}" recorded_payload_files ${index} size)
    string(JSON payload_hash GET "${verified_record}" recorded_payload_files ${index} sha256)
    file(SIZE "${tampered}/${payload_path}" actual_size)
    file(SHA256 "${tampered}/${payload_path}" actual_hash)
    if(NOT actual_size EQUAL payload_size OR NOT actual_hash STREQUAL payload_hash)
        message(FATAL_ERROR "${case_name}: Record payload association is inconsistent")
    endif()
endforeach()
file(STRINGS "${tampered}/SHA256SUMS" manifest_lines)
set(record_bound OFF)
set(result_bound OFF)
foreach(line IN LISTS manifest_lines)
    if(NOT line MATCHES "^([0-9a-f]+)  (.+)$")
        message(FATAL_ERROR "${case_name}: malformed manifest line")
    endif()
    set(recorded_hash "${CMAKE_MATCH_1}")
    set(recorded_path "${CMAKE_MATCH_2}")
    file(SHA256 "${tampered}/${recorded_path}" actual_hash)
    if(NOT actual_hash STREQUAL recorded_hash)
        message(FATAL_ERROR "${case_name}: manifest association is inconsistent")
    endif()
    if(recorded_path STREQUAL "run_record_v0.5.json")
        set(record_bound ON)
    elseif(recorded_path STREQUAL "result_summary_v0.5.json")
        set(result_bound ON)
    endif()
endforeach()
if(NOT record_bound OR NOT result_bound)
    message(FATAL_ERROR "${case_name}: manifest does not bind Record and Result")
endif()
run_lab(3 rejected compare --left-run "${tampered}" --right-run "${tampered}" --output json)
file(WRITE "${PAE_LAB_RUN_ROOT}/${case_name}-rejection.json" "${rejected}")
assert_json("${rejected}" "diagnostic;id" "PAE_LAB_COMPARE_RUN_ERROR")
string(JSON rejection_detail GET "${rejected}" diagnostic detail)
if(NOT rejection_detail STREQUAL expected_detail)
    message(FATAL_ERROR "semantic tamper hit wrong rejection: ${rejection_detail}")
endif()
message(STATUS "Self-consistent ${case_name}: ${rejection_detail}")
endfunction()

assert_semantic_tamper(unknown-kind [["kind":"INT64"]] [["kind":"UNKNOWN"]]
                      "stored field kind is unsupported")
set(valid_tuple [["kind":"INT64","raw_value":"-2","logical_value":"-2","enum_known":false]])
set(tuple_diagnostic "stored INT64 field has an invalid raw/logical/enum_known tuple")
foreach(invalid IN ITEMS "-0" "01" "+1" "9223372036854775808" "-9223372036854775809")
    string(MAKE_C_IDENTIFIER "${invalid}" case_id)
    assert_semantic_tamper("int64-${case_id}" "${valid_tuple}"
        "\"kind\":\"INT64\",\"raw_value\":\"${invalid}\",\"logical_value\":\"${invalid}\",\"enum_known\":false"
        "${tuple_diagnostic}")
endforeach()
assert_semantic_tamper(int64-mismatch "${valid_tuple}"
    [["kind":"INT64","raw_value":"-2","logical_value":"-3","enum_known":false]] "${tuple_diagnostic}")
assert_semantic_tamper(int64-enum-known "${valid_tuple}"
    [["kind":"INT64","raw_value":"-2","logical_value":"-2","enum_known":true]] "${tuple_diagnostic}")

message(STATUS "PAE DEC-042A Lab generation contract passed")
