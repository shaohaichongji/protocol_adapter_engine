if(NOT DEFINED PAE_LAB_EXECUTABLE OR NOT DEFINED PAE_LAB_DATA_DIR OR
   NOT DEFINED PAE_LAB_RUN_ROOT)
    message(FATAL_ERROR "Schema 0.9 gate test arguments are incomplete")
endif()

set(config "${PAE_LAB_DATA_DIR}/synthetic_stream_framing_slice.pae.json")
set(frame "${PAE_LAB_DATA_DIR}/lab_command_001.frame.hex")
set(values "${PAE_LAB_DATA_DIR}/valid_command.values.pae-lab.json")

function(run_gate_case name)
    execute_process(
        COMMAND "${PAE_LAB_EXECUTABLE}" ${ARGN} --output json
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT exit_code EQUAL 4)
        message(FATAL_ERROR "${name}: expected exit 4, got ${exit_code}: ${output}${error}")
    endif()
    string(JSON status GET "${output}" operation_status)
    string(JSON diagnostic GET "${output}" diagnostic id)
    string(JSON evidence GET "${output}" evidence_bundle)
    if(NOT status STREQUAL "CONFIG_COMPILE_FAILED" OR
       NOT diagnostic STREQUAL "PAE_LAB_CONFIG_COMPILE_FAILED" OR
       NOT evidence STREQUAL "")
        message(FATAL_ERROR "${name}: Schema 0.9 gate semantics differ: ${output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${PAE_LAB_RUN_ROOT}")
file(MAKE_DIRECTORY "${PAE_LAB_RUN_ROOT}")
run_gate_case(inspect-no-record inspect --config "${config}" --frame-hex "${frame}")
run_gate_case(encode-no-record encode --config "${config}" --values "${values}")
run_gate_case(inspect-record inspect --config "${config}" --frame-hex "${frame}"
              --record-root "${PAE_LAB_RUN_ROOT}/inspect")
run_gate_case(encode-record encode --config "${config}" --values "${values}"
              --record-root "${PAE_LAB_RUN_ROOT}/encode")
run_gate_case(udp-record udp-exchange --config "${config}" --values "${values}"
              --remote 127.0.0.1:9 --receive-pipeline sync_fixed_rx
              --record-root "${PAE_LAB_RUN_ROOT}/udp")

file(GLOB_RECURSE published_files LIST_DIRECTORIES false "${PAE_LAB_RUN_ROOT}/*")
if(published_files)
    message(FATAL_ERROR "Schema 0.9 rejection gate published evidence files: ${published_files}")
endif()

message(STATUS "Schema 0.9 Lab rejection gate passed without evidence publication")
