if(NOT DEFINED PAE_JSON_SPIKE_SOURCE_FIXTURE_DIR OR
   NOT IS_DIRECTORY "${PAE_JSON_SPIKE_SOURCE_FIXTURE_DIR}")
    message(FATAL_ERROR "PAE_JSON_SPIKE_SOURCE_FIXTURE_DIR must name an existing directory")
endif()
if(NOT DEFINED PAE_JSON_SPIKE_WORK_DIR OR PAE_JSON_SPIKE_WORK_DIR STREQUAL "")
    message(FATAL_ERROR "PAE_JSON_SPIKE_WORK_DIR is required")
endif()
if(NOT DEFINED PAE_JSON_SPIKE_MUTATION OR PAE_JSON_SPIKE_MUTATION STREQUAL "")
    message(FATAL_ERROR "PAE_JSON_SPIKE_MUTATION is required")
endif()
if(NOT DEFINED PAE_JSON_SPIKE_EXPECTED_DIAGNOSTIC OR
   PAE_JSON_SPIKE_EXPECTED_DIAGNOSTIC STREQUAL "")
    message(FATAL_ERROR "PAE_JSON_SPIKE_EXPECTED_DIAGNOSTIC is required")
endif()

set(_fixture_files
    limits_v0.1.tsv
    strict_json_corpus_v0.1.tsv
    strict_json_inventory_v0.1.tsv
    number_token_corpus_v0.1.tsv
    number_token_inventory_v0.1.tsv
    resource_profiles_v0.1.tsv
)
file(MAKE_DIRECTORY "${PAE_JSON_SPIKE_WORK_DIR}/fixtures")
foreach(_fixture_file IN LISTS _fixture_files)
    file(COPY_FILE
        "${PAE_JSON_SPIKE_SOURCE_FIXTURE_DIR}/${_fixture_file}"
        "${PAE_JSON_SPIKE_WORK_DIR}/fixtures/${_fixture_file}"
    )
endforeach()

set(_strict_path "${PAE_JSON_SPIKE_WORK_DIR}/fixtures/strict_json_corpus_v0.1.tsv")
set(_inventory_path "${PAE_JSON_SPIKE_WORK_DIR}/fixtures/strict_json_inventory_v0.1.tsv")
set(_base_prefix
    "negative_${PAE_JSON_SPIKE_MUTATION}\tSJ-NEG-001\tGENERATOR_NEGATIVE\tCONFORMANCE\t7B7D\tspike_default\tSYNTAX_ERROR")

if(PAE_JSON_SPIKE_MUTATION STREQUAL "exact_parser_reported")
    set(_row "${_base_prefix}\t-\t-\tABSENT\t-\tEXACT\tPARSER_REPORTED_POSITION\t0\t2\n")
elseif(PAE_JSON_SPIKE_MUTATION STREQUAL "exact_outside_input")
    set(_row "${_base_prefix}\t-\t-\tABSENT\t-\tEXACT\tEXACT_INPUT_BYTE\t2\t2\n")
elseif(PAE_JSON_SPIKE_MUTATION STREQUAL "kind_only_none")
    set(_row "${_base_prefix}\t-\t-\tABSENT\t-\tKIND_ONLY\tNONE\t-\t2\n")
elseif(PAE_JSON_SPIKE_MUTATION STREQUAL "non_characterization_target")
    set(_row "${_base_prefix}\tOK\tyyjson\tABSENT\t-\tABSENT\tNONE\t-\t2\n")
elseif(PAE_JSON_SPIKE_MUTATION STREQUAL "missing_inventory")
    set(_row "${_base_prefix}\t-\t-\tABSENT\t-\tABSENT\tNONE\t-\t2\n")
else()
    message(FATAL_ERROR "Unknown corpus mutation '${PAE_JSON_SPIKE_MUTATION}'")
endif()

if(NOT PAE_JSON_SPIKE_MUTATION STREQUAL "missing_inventory")
    file(APPEND "${_inventory_path}"
        "negative_${PAE_JSON_SPIKE_MUTATION}\tMANIFEST_AUTHORITY\n")
endif()
file(APPEND "${_strict_path}" "${_row}")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_JSON_SPIKE_FIXTURE_DIR=${PAE_JSON_SPIKE_WORK_DIR}/fixtures"
        "-DPAE_JSON_SPIKE_OUTPUT_DIR=${PAE_JSON_SPIKE_WORK_DIR}/generated"
        -P "${CMAKE_CURRENT_LIST_DIR}/ValidateCorpusStandalone.cmake"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
)

if(_result EQUAL 0)
    message(FATAL_ERROR
        "Mutation '${PAE_JSON_SPIKE_MUTATION}' was unexpectedly accepted")
endif()
set(_combined_output "${_stdout}\n${_stderr}")
string(FIND "${_combined_output}" "${PAE_JSON_SPIKE_EXPECTED_DIAGNOSTIC}" _diagnostic_index)
if(_diagnostic_index EQUAL -1)
    message(FATAL_ERROR
        "Mutation '${PAE_JSON_SPIKE_MUTATION}' failed for an unexpected reason:\n"
        "${_combined_output}")
endif()

message(STATUS
    "Mutation '${PAE_JSON_SPIKE_MUTATION}' rejected with the expected diagnostic")
