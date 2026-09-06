if(NOT DEFINED PAE_LAB_EXECUTABLE OR NOT DEFINED PAE_LAB_DATA_DIR OR
   NOT DEFINED PAE_LAB_RUN_ROOT)
    message(FATAL_ERROR "DEC-041 Lab test arguments are incomplete")
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

function(copy_mutate_v4_file source destination relative_file from to)
    file(REMOVE_RECURSE "${destination}")
    file(MAKE_DIRECTORY "${destination}")
    file(COPY "${source}/" DESTINATION "${destination}")
    set(target_file "${destination}/${relative_file}")
    set(record_file "${destination}/run_record_v0.4.json")
    file(SIZE "${target_file}" old_target_size)
    file(SHA256 "${target_file}" old_target_hash)
    file(SHA256 "${record_file}" old_record_hash)
    file(READ "${target_file}" target_text)
    string(FIND "${target_text}" "${from}" mutation_position)
    if(mutation_position EQUAL -1)
        message(FATAL_ERROR "V0.4 mutation marker not found: ${from}")
    endif()
    string(REPLACE "${from}" "${to}" target_text "${target_text}")
    file(WRITE "${target_file}" "${target_text}")
    file(SIZE "${target_file}" new_target_size)
    file(SHA256 "${target_file}" new_target_hash)
    file(READ "${record_file}" record_text)
    set(old_payload
        "{\"path\":\"${relative_file}\",\"size\":${old_target_size},\"sha256\":\"${old_target_hash}\"}")
    set(new_payload
        "{\"path\":\"${relative_file}\",\"size\":${new_target_size},\"sha256\":\"${new_target_hash}\"}")
    string(FIND "${record_text}" "${old_payload}" payload_position)
    if(payload_position EQUAL -1)
        message(FATAL_ERROR "V0.4 Run Record payload entry was not found for ${relative_file}")
    endif()
    string(REPLACE "${old_payload}" "${new_payload}" record_text "${record_text}")
    file(WRITE "${record_file}" "${record_text}")
    file(SHA256 "${record_file}" new_record_hash)
    file(READ "${destination}/SHA256SUMS" sums)
    string(REPLACE "${old_target_hash}" "${new_target_hash}" sums "${sums}")
    string(REPLACE "${old_record_hash}" "${new_record_hash}" sums "${sums}")
    file(WRITE "${destination}/SHA256SUMS" "${sums}")

    # Prove the mutated fixture is self-consistent at the byte-accounting and Hash layers before
    # asking the Reader to exercise the intended semantic rejection path.
    file(SIZE "${target_file}" verified_target_size)
    file(SHA256 "${target_file}" verified_target_hash)
    file(SHA256 "${record_file}" verified_record_hash)
    file(READ "${record_file}" verified_record)
    file(READ "${destination}/SHA256SUMS" verified_sums)
    string(FIND "${verified_record}" "${new_payload}" verified_payload_position)
    string(FIND "${verified_sums}" "${verified_target_hash}  ${relative_file}"
           verified_target_manifest_position)
    string(FIND "${verified_sums}" "${verified_record_hash}  run_record_v0.4.json"
           verified_record_manifest_position)
    if(NOT verified_target_size EQUAL new_target_size OR
       NOT verified_target_hash STREQUAL new_target_hash OR
       verified_payload_position EQUAL -1 OR
       verified_target_manifest_position EQUAL -1 OR
       verified_record_manifest_position EQUAL -1)
        message(FATAL_ERROR "V0.4 mutated Bundle is not self-consistent for ${relative_file}")
    endif()
endfunction()

set(config "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.pae.json")
set(values "${PAE_LAB_DATA_DIR}/synthetic_sum8_slice.values.pae-lab.json")
set(frame "${PAE_LAB_DATA_DIR}/sum8_record_001.frame.hex")

file(READ "${values}" extra_value_text)
string(REPLACE
       "    {\"id\": \"payload_a\", \"kind\": \"UINT64\", \"uint64\": \"240\"}"
       "    {\"id\": \"payload_a\", \"kind\": \"UINT64\", \"uint64\": \"240\"},\n    {\"id\": \"integrity\", \"kind\": \"UINT64\", \"uint64\": \"19\"}"
       extra_value_text "${extra_value_text}")
file(WRITE "${PAE_LAB_RUN_ROOT}/integrity-input.values.json" "${extra_value_text}")
run_lab(5 extra_value encode --config "${config}"
        --values "${PAE_LAB_RUN_ROOT}/integrity-input.values.json" --output json)
assert_json("${extra_value}" operation_status "VALUES_INVALID")
assert_json("${extra_value}" "diagnostic;id" "PAE_LAB_VALUES_UNKNOWN_FIELD")
assert_json("${extra_value}" format_version "pae.lab.result/0.4")

file(WRITE "${PAE_LAB_RUN_ROOT}/unknown.hex" "70\n")
run_lab(5 unknown inspect --config "${config}"
        --frame-hex "${PAE_LAB_RUN_ROOT}/unknown.hex" --output json)
assert_json("${unknown}" operation_status "UNKNOWN_MESSAGE")
assert_json("${unknown}" format_version "pae.lab.result/0.4")

file(WRITE "${PAE_LAB_RUN_ROOT}/ambiguous.hex" "01 02 01\n")
run_lab(5 ambiguous inspect
        --config "${PAE_LAB_DATA_DIR}/sum8_cross_pipeline_ambiguous.pae.json"
        --frame-hex "${PAE_LAB_RUN_ROOT}/ambiguous.hex" --output json)
assert_json("${ambiguous}" operation_status "AMBIGUOUS_MESSAGE")
assert_json("${ambiguous}" "diagnostic;id" "PAE_LAB_CODEC_AMBIGUOUS_MESSAGE")
assert_json("${ambiguous}" format_version "pae.lab.result/0.4")

set(encode_root "${PAE_LAB_RUN_ROOT}/encode")
run_lab(0 encoded encode --config "${config}" --values "${values}"
        --record-root "${encode_root}" --output json)
assert_json("${encoded}" format_version "pae.lab.result/0.4")
assert_json("${encoded}" operation_status "OK")
assert_json("${encoded}" frame_hex "70F0200102137E")
assert_json("${encoded}" replay_mode "ENCODE_TX")
only_bundle("${encode_root}" encode_bundle)
if(NOT EXISTS "${encode_bundle}/result_summary_v0.4.json" OR
   NOT EXISTS "${encode_bundle}/run_record_v0.4.json" OR
   NOT EXISTS "${encode_bundle}/events_v0.4.jsonl")
    message(FATAL_ERROR "Schema 0.3 Bundle does not contain the 0.4 file set")
endif()

set(unknown_format_bundle "${PAE_LAB_RUN_ROOT}/unknown-format")
copy_mutate_v4_file("${encode_bundle}" "${unknown_format_bundle}" "result_summary_v0.4.json"
                    "pae.lab.result/0.4" "pae.lab.result/0.5")
run_lab(3 unknown_format replay --bundle "${unknown_format_bundle}" --output json)
assert_json("${unknown_format}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")

set(unknown_event_bundle "${PAE_LAB_RUN_ROOT}/unknown-event-field")
copy_mutate_v4_file("${encode_bundle}" "${unknown_event_bundle}" "events_v0.4.jsonl"
                    "\"event_kind\"" "\"unexpected\":0,\"event_kind\"")
run_lab(3 unknown_event replay --bundle "${unknown_event_bundle}" --output json)
assert_json("${unknown_event}" "diagnostic;id" "PAE_LAB_RUN_INPUT_ERROR")
assert_json("${unknown_event}" "diagnostic;detail"
            "V0.2 event contains unknown property unexpected")

set(replay_a_root "${PAE_LAB_RUN_ROOT}/encode-replay-a")
run_lab(0 replay_a replay --bundle "${encode_bundle}" --record-root "${replay_a_root}" --output json)
assert_json("${replay_a}" comparison_status "EQUAL")
assert_json("${replay_a}" comparison_equal "ON")
only_bundle("${replay_a_root}" replay_a_bundle)
run_lab(0 replay_b replay --bundle "${replay_a_bundle}"
        --record-root "${PAE_LAB_RUN_ROOT}/encode-replay-b" --output json)
assert_json("${replay_b}" comparison_status "EQUAL")

file(WRITE "${PAE_LAB_RUN_ROOT}/covered-corrupt.hex" "70 F1 20 01 02 13 7E\n")
set(failure_root "${PAE_LAB_RUN_ROOT}/failure")
run_lab(5 failure inspect --config "${config}"
        --frame-hex "${PAE_LAB_RUN_ROOT}/covered-corrupt.hex"
        --record-root "${failure_root}" --output json)
assert_json("${failure}" format_version "pae.lab.result/0.4")
assert_json("${failure}" operation_status "INTEGRITY_FAILED")
assert_json("${failure}" "diagnostic;id" "PAE_LAB_CODEC_INTEGRITY_FAILED")
assert_json("${failure}" "fields" "[]")
assert_json("${failure}" current_execution_status "INTEGRITY_FAILED")
only_bundle("${failure_root}" failure_bundle)

file(SHA256 "${failure_bundle}/SHA256SUMS" source_manifest_before)
set(failure_a_root "${PAE_LAB_RUN_ROOT}/failure-replay-a")
run_lab(5 failure_a replay --bundle "${failure_bundle}" --record-root "${failure_a_root}" --output json)
assert_json("${failure_a}" current_execution_status "INTEGRITY_FAILED")
assert_json("${failure_a}" comparison_status "EQUAL")
assert_json("${failure_a}" comparison_equal "ON")
only_bundle("${failure_a_root}" failure_a_bundle)
run_lab(5 failure_b replay --bundle "${failure_a_bundle}"
        --record-root "${PAE_LAB_RUN_ROOT}/failure-replay-b" --output json)
assert_json("${failure_b}" current_execution_status "INTEGRITY_FAILED")
assert_json("${failure_b}" comparison_status "EQUAL")
file(SHA256 "${failure_bundle}/SHA256SUMS" source_manifest_after)
if(NOT source_manifest_before STREQUAL source_manifest_after)
    message(FATAL_ERROR "Replay modified the source Bundle")
endif()

file(READ "${config}" changed_text)
string(REPLACE "\"range\": {\"byte_offset\": 1, \"byte_length\": 4}"
               "\"range\": {\"byte_offset\": 2, \"byte_length\": 3}" changed_text "${changed_text}")
set(changed "${PAE_LAB_RUN_ROOT}/changed.pae.json")
file(WRITE "${changed}" "${changed_text}")
set(changed_root "${PAE_LAB_RUN_ROOT}/changed-replay")
run_lab(6 changed_replay replay --bundle "${encode_bundle}" --config "${changed}"
        --record-root "${changed_root}" --output json)
assert_json("${changed_replay}" comparison_status "DIFFERENT")
assert_json("${changed_replay}" comparison_equal "OFF")
assert_json("${changed_replay}" cross_config_replay "ON")
assert_json("${changed_replay}" frame_hex "70F0200102237E")
only_bundle("${changed_root}" changed_bundle)
run_lab(0 changed_b replay --bundle "${changed_bundle}"
        --record-root "${PAE_LAB_RUN_ROOT}/changed-replay-b" --output json)
assert_json("${changed_b}" comparison_status "EQUAL")

file(READ "${config}" no_integrity_text)
string(REPLACE "          {\"kind\": \"fixed_bytes\", \"byte_offset\": 6, \"bytes\": \"7E\"}"
               "          {\"kind\": \"fixed_bytes\", \"byte_offset\": 5, \"bytes\": \"13\"},\n          {\"kind\": \"fixed_bytes\", \"byte_offset\": 6, \"bytes\": \"7E\"}"
               no_integrity_text "${no_integrity_text}")
string(REPLACE "      \"integrity\": {\n        \"algorithm\": \"sum8\",\n        \"range\": {\"byte_offset\": 1, \"byte_length\": 4},\n        \"storage\": {\"byte_offset\": 5}\n      },\n"
               "" no_integrity_text "${no_integrity_text}")
set(no_integrity "${PAE_LAB_RUN_ROOT}/no-integrity.pae.json")
file(WRITE "${no_integrity}" "${no_integrity_text}")
run_lab(0 no_integrity_result encode --config "${no_integrity}" --values "${values}" --output json)
assert_json("${no_integrity_result}" format_version "pae.lab.result/0.4")
assert_json("${no_integrity_result}" frame_hex "70F0200102137E")

set(old_root "${PAE_LAB_RUN_ROOT}/old")
run_lab(0 old_result encode
        --config "${PAE_LAB_DATA_DIR}/synthetic_lab_exchange_slice.pae.json"
        --values "${PAE_LAB_DATA_DIR}/valid_command.values.pae-lab.json"
        --record-root "${old_root}" --output json)
assert_json("${old_result}" format_version "pae.lab.result/0.1")
only_bundle("${old_root}" old_bundle)
run_lab(3 cross_replay replay --bundle "${old_bundle}" --config "${config}" --output json)
assert_json("${cross_replay}" "diagnostic;id" "PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED")
assert_json("${cross_replay}" format_version "pae.lab.result/0.4")
assert_json("${cross_replay}" comparison_status "NOT_EVALUATED")
run_lab(3 reverse_cross_replay replay --bundle "${encode_bundle}"
        --config "${PAE_LAB_DATA_DIR}/synthetic_lab_exchange_slice.pae.json" --output json)
assert_json("${reverse_cross_replay}" "diagnostic;id" "PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED")
assert_json("${reverse_cross_replay}" format_version "pae.lab.result/0.1")
run_lab(3 cross_compare compare --left-run "${old_bundle}" --right-run "${encode_bundle}" --output json)
assert_json("${cross_compare}" "diagnostic;id" "PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED")
assert_json("${cross_compare}" operation_status "INPUT_ERROR")
run_lab(0 raw_cross_generation compare --left-frame-hex "${frame}"
        --right-frame-bin "${encode_bundle}/frames/000001_frame.bin" --output json)
assert_json("${raw_cross_generation}" operation_status "EQUAL")
assert_json("${raw_cross_generation}" comparison_equal "ON")

message(STATUS "PAE_DEC041_LAB_GENERATION_PASS")
