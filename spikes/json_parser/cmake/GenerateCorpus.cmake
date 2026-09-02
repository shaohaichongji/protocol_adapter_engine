function(pae_require_member value allowed_values description)
    if(NOT value IN_LIST allowed_values)
        message(FATAL_ERROR "Invalid ${description} '${value}'")
    endif()
endfunction()

function(pae_read_lf_tsv path expected_header output_lines)
    file(READ "${path}" _content)
    string(FIND "${_content}" "\r" _carriage_return)
    if(NOT _carriage_return EQUAL -1)
        message(FATAL_ERROR "Corpus must use LF line endings: ${path}")
    endif()

    string(REPLACE "\n" ";" _lines "${_content}")
    list(POP_FRONT _lines _actual_header)
    if(NOT _actual_header STREQUAL expected_header)
        message(FATAL_ERROR "Unexpected TSV header in ${path}")
    endif()

    set(_nonempty_lines)
    foreach(_line IN LISTS _lines)
        if(NOT _line STREQUAL "")
            list(APPEND _nonempty_lines "${_line}")
        endif()
    endforeach()
    set(${output_lines} "${_nonempty_lines}" PARENT_SCOPE)
endfunction()

function(pae_generate_json_spike_corpora output_dir)
    if(ARGC GREATER 1)
        set(_fixture_dir "${ARGV1}")
    else()
        set(_fixture_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../fixtures")
    endif()
    set(_limits_path "${_fixture_dir}/limits_v0.1.tsv")
    set(_strict_path "${_fixture_dir}/strict_json_corpus_v0.1.tsv")
    set(_inventory_path "${_fixture_dir}/strict_json_inventory_v0.1.tsv")
    set(_number_path "${_fixture_dir}/number_token_corpus_v0.1.tsv")
    set(_number_inventory_path "${_fixture_dir}/number_token_inventory_v0.1.tsv")
    set(_resource_profile_path "${_fixture_dir}/resource_profiles_v0.1.tsv")
    set_property(
        DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_limits_path}"
        "${_strict_path}"
        "${_inventory_path}"
        "${_number_path}"
        "${_number_inventory_path}"
        "${_resource_profile_path}"
    )

    pae_read_lf_tsv(
        "${_inventory_path}"
        "case_id\tsource_state"
        _inventory_lines
    )
    set(_valid_source_states LEGACY_EMBEDDED MANIFEST_AUTHORITY)
    set(_inventory_case_ids)
    set(_manifest_authority_ids)
    set(PAE_STRICT_JSON_INVENTORY_RECORDS "")
    foreach(_line IN LISTS _inventory_lines)
        string(REPLACE "\t" ";" _fields "${_line}")
        list(LENGTH _fields _field_count)
        if(NOT _field_count EQUAL 2)
            message(FATAL_ERROR "Inventory row must contain 2 fields: ${_line}")
        endif()
        list(GET _fields 0 _case_id)
        list(GET _fields 1 _source_state)
        if(NOT _case_id MATCHES "^[a-z][a-z0-9_]*$" OR _case_id IN_LIST _inventory_case_ids)
            message(FATAL_ERROR "Invalid or duplicate inventory case_id '${_case_id}'")
        endif()
        pae_require_member("${_source_state}" "${_valid_source_states}"
                           "source_state for ${_case_id}")
        list(APPEND _inventory_case_ids "${_case_id}")
        if(_source_state STREQUAL "MANIFEST_AUTHORITY")
            list(APPEND _manifest_authority_ids "${_case_id}")
        endif()
        string(APPEND PAE_STRICT_JSON_INVENTORY_RECORDS
            "  {\"${_case_id}\", \"${_source_state}\"},\n")
    endforeach()

    pae_read_lf_tsv(
        "${_resource_profile_path}"
        "profile_id\tstatus\tmax_input_bytes\tmax_depth\tmax_nodes\tmax_object_members\tmax_string_bytes\tmax_array_elements\tmax_number_token_bytes\tmax_total_string_bytes\tmax_parser_memory_bytes\tmax_loader_compiler_peak_bytes"
        _resource_profile_lines
    )
    set(_resource_profile_ids)
    set(PAE_RESOURCE_PROFILE_RECORDS "")
    foreach(_line IN LISTS _resource_profile_lines)
        string(REPLACE "\t" ";" _fields "${_line}")
        list(LENGTH _fields _field_count)
        if(NOT _field_count EQUAL 12)
            message(FATAL_ERROR "Resource profile row must contain 12 fields: ${_line}")
        endif()
        list(GET _fields 0 _profile_id)
        list(GET _fields 1 _profile_status)
        if(NOT _profile_id MATCHES "^[a-z][a-z0-9_]*$" OR
           _profile_id IN_LIST _resource_profile_ids)
            message(FATAL_ERROR "Invalid or duplicate resource profile '${_profile_id}'")
        endif()
        if(NOT _profile_status STREQUAL "CANDIDATE_UNVERIFIED")
            message(FATAL_ERROR "Resource profile must remain CANDIDATE_UNVERIFIED")
        endif()
        list(APPEND _resource_profile_ids "${_profile_id}")
        foreach(_index RANGE 2 11)
            list(GET _fields ${_index} _number)
            if(NOT _number MATCHES "^[1-9][0-9]*$" OR _number GREATER 2147483647)
                message(FATAL_ERROR "Invalid resource limit in '${_profile_id}'")
            endif()
        endforeach()
        list(GET _fields 2 _max_input)
        list(GET _fields 3 _max_depth)
        list(GET _fields 4 _max_nodes)
        list(GET _fields 5 _max_object)
        list(GET _fields 6 _max_string)
        list(GET _fields 7 _max_array)
        list(GET _fields 8 _max_number)
        list(GET _fields 9 _max_total_string)
        list(GET _fields 10 _max_parser_memory)
        list(GET _fields 11 _max_loader_compiler_peak)
        if(_max_parser_memory LESS _max_input OR
           _max_loader_compiler_peak LESS _max_parser_memory OR
           _max_total_string LESS _max_string)
            message(FATAL_ERROR "Inconsistent resource budget in '${_profile_id}'")
        endif()
        string(APPEND PAE_RESOURCE_PROFILE_RECORDS
            "  {\"${_profile_id}\", \"${_profile_status}\", ${_max_input}ULL, "
            "${_max_depth}ULL, ${_max_nodes}ULL, ${_max_object}ULL, ${_max_string}ULL, "
            "${_max_array}ULL, ${_max_number}ULL, ${_max_total_string}ULL, "
            "${_max_parser_memory}ULL, ${_max_loader_compiler_peak}ULL},\n")
    endforeach()
    foreach(_required_profile hard desktop constrained)
        if(NOT _required_profile IN_LIST _resource_profile_ids)
            message(FATAL_ERROR "Missing required resource profile '${_required_profile}'")
        endif()
    endforeach()
    list(LENGTH _resource_profile_ids _resource_profile_count)
    if(NOT _resource_profile_count EQUAL 3)
        message(FATAL_ERROR "Resource profile manifest must contain exactly three profiles")
    endif()

    pae_read_lf_tsv(
        "${_limits_path}"
        "limits_id\tmax_input_bytes\tmax_depth\tmax_nodes\tmax_string_bytes\tmax_array_elements\tmax_parser_memory_bytes"
        _limit_lines
    )
    set(_limit_ids)
    set(PAE_STRICT_JSON_LIMIT_RECORDS "")
    foreach(_line IN LISTS _limit_lines)
        string(REPLACE "\t" ";" _fields "${_line}")
        list(LENGTH _fields _field_count)
        if(NOT _field_count EQUAL 7)
            message(FATAL_ERROR "Limit corpus row must contain 7 fields: ${_line}")
        endif()
        list(GET _fields 0 _id)
        if(NOT _id MATCHES "^[a-z][a-z0-9_]*$")
            message(FATAL_ERROR "Invalid limits_id '${_id}'")
        endif()
        if(_id IN_LIST _limit_ids)
            message(FATAL_ERROR "Duplicate limits_id '${_id}'")
        endif()
        list(APPEND _limit_ids "${_id}")
        foreach(_index RANGE 1 6)
            list(GET _fields ${_index} _number)
            if(NOT _number MATCHES "^(0|[1-9][0-9]*)$")
                message(FATAL_ERROR "Non-decimal limit in '${_id}'")
            endif()
            if(_number GREATER 2147483647)
                message(FATAL_ERROR "Limit exceeds the Spike generator range in '${_id}'")
            endif()
        endforeach()
        list(GET _fields 1 _max_input)
        list(GET _fields 2 _max_depth)
        list(GET _fields 3 _max_nodes)
        list(GET _fields 4 _max_string)
        list(GET _fields 5 _max_array)
        list(GET _fields 6 _max_memory)
        string(APPEND PAE_STRICT_JSON_LIMIT_RECORDS
            "  {\"${_id}\", ${_max_input}ULL, ${_max_depth}ULL, ${_max_nodes}ULL, "
            "${_max_string}ULL, ${_max_array}ULL, ${_max_memory}ULL},\n")
    endforeach()

    pae_read_lf_tsv(
        "${_strict_path}"
        "case_id\trule_id\tsuite\tstatus\tinput_hex\tlimits_id\texpected_code\ttarget_code\ttarget_lock_candidate\tpointer_assertion\tpointer_utf8_hex\toffset_assertion\toffset_kind\toffset_value\texpected_input_size"
        _strict_lines
    )
    set(_valid_codes
        OK INPUT_LIMIT BOM_NOT_ALLOWED INVALID_UTF8 SYNTAX_ERROR DUPLICATE_KEY DEPTH_LIMIT
        NODE_LIMIT OBJECT_LIMIT STRING_LIMIT ARRAY_LIMIT NUMBER_TOKEN_LIMIT TOTAL_STRING_LIMIT
        INTEGER_NOT_EXACT INTEGER_OUT_OF_RANGE REAL_OUT_OF_RANGE UNKNOWN_FIELD
        MISSING_FIELD TYPE_MISMATCH REFERENCE_NOT_FOUND SEMANTIC_CONSTRAINT RESOURCE_EXHAUSTED
        INTERNAL_ERROR
    )
    set(_valid_statuses CONFORMANCE CHARACTERIZATION OPEN_DECISION)
    set(_valid_target_lock_candidates - yyjson nlohmann_json rapidjson)
    set(_valid_pointer_assertions ABSENT ROOT EXACT IGNORE)
    set(_valid_offset_assertions ABSENT EXACT KIND_ONLY IGNORE)
    set(_valid_offset_kinds NONE EXACT_INPUT_BYTE PARSER_REPORTED_POSITION)
    set(_case_ids)
    set(PAE_STRICT_JSON_FIXTURE_RECORDS "")
    foreach(_line IN LISTS _strict_lines)
        string(REPLACE "\t" ";" _fields "${_line}")
        list(LENGTH _fields _field_count)
        if(NOT _field_count EQUAL 15)
            message(FATAL_ERROR "Strict JSON corpus row must contain 15 fields: ${_line}")
        endif()
        list(GET _fields 0 _case_id)
        list(GET _fields 1 _rule_id)
        list(GET _fields 2 _suite)
        list(GET _fields 3 _status)
        list(GET _fields 4 _input_hex)
        list(GET _fields 5 _limits_id)
        list(GET _fields 6 _expected_code)
        list(GET _fields 7 _target_code)
        list(GET _fields 8 _target_lock_candidate)
        list(GET _fields 9 _pointer_assertion)
        list(GET _fields 10 _pointer_hex)
        list(GET _fields 11 _offset_assertion)
        list(GET _fields 12 _offset_kind)
        list(GET _fields 13 _offset_value)
        list(GET _fields 14 _expected_size)

        if(NOT _case_id MATCHES "^[a-z][a-z0-9_]*$" OR _case_id IN_LIST _case_ids)
            message(FATAL_ERROR "Invalid or duplicate case_id '${_case_id}'")
        endif()
        if(NOT _case_id IN_LIST _manifest_authority_ids)
            message(FATAL_ERROR "Strict corpus case '${_case_id}' is missing MANIFEST_AUTHORITY inventory state")
        endif()
        list(APPEND _case_ids "${_case_id}")
        if(NOT _rule_id MATCHES "^[A-Z0-9-]+$" OR NOT _suite MATCHES "^[A-Z_]+$")
            message(FATAL_ERROR "Invalid rule_id or suite for '${_case_id}'")
        endif()
        pae_require_member("${_status}" "${_valid_statuses}" "status for ${_case_id}")
        pae_require_member("${_expected_code}" "${_valid_codes}" "expected_code for ${_case_id}")
        pae_require_member("${_target_lock_candidate}" "${_valid_target_lock_candidates}"
                           "target_lock_candidate for ${_case_id}")
        pae_require_member("${_pointer_assertion}" "${_valid_pointer_assertions}"
                           "pointer_assertion for ${_case_id}")
        pae_require_member("${_offset_assertion}" "${_valid_offset_assertions}"
                           "offset_assertion for ${_case_id}")
        pae_require_member("${_offset_kind}" "${_valid_offset_kinds}"
                           "offset_kind for ${_case_id}")
        if(NOT _limits_id IN_LIST _limit_ids)
            message(FATAL_ERROR "Unknown limits_id '${_limits_id}' for '${_case_id}'")
        endif()
        if(NOT _expected_size MATCHES "^(0|[1-9][0-9]*)$")
            message(FATAL_ERROR "Invalid input bytes or size for '${_case_id}'")
        endif()
        if(_expected_size GREATER 2147483647)
            message(FATAL_ERROR "Input size exceeds the Spike generator range for '${_case_id}'")
        endif()
        if(_input_hex STREQUAL "-")
            if(NOT _expected_size EQUAL 0)
                message(FATAL_ERROR "Empty input marker has a non-zero size for '${_case_id}'")
            endif()
            set(_hex_length 0)
        elseif(_input_hex MATCHES "^([0-9A-F][0-9A-F])+$")
            string(LENGTH "${_input_hex}" _hex_length)
        else()
            message(FATAL_ERROR "Invalid input bytes or size for '${_case_id}'")
        endif()
        math(EXPR _expected_hex_length "${_expected_size} * 2")
        if(NOT _hex_length EQUAL _expected_hex_length)
            message(FATAL_ERROR "Input size mismatch for '${_case_id}'")
        endif()

        if(_status STREQUAL "CHARACTERIZATION")
            pae_require_member("${_target_code}" "${_valid_codes}" "target_code for ${_case_id}")
            if("${_target_code}" STREQUAL "${_expected_code}")
                message(FATAL_ERROR "Characterization target equals current code for '${_case_id}'")
            endif()
        elseif(NOT _target_code STREQUAL "-" OR NOT _target_lock_candidate STREQUAL "-")
            message(FATAL_ERROR
                "Only characterization cases may define target_code or target lock: ${_case_id}")
        endif()
        if(_pointer_assertion STREQUAL "EXACT")
            if(NOT _pointer_hex MATCHES "^([0-9A-F][0-9A-F])+$")
                message(FATAL_ERROR "Invalid pointer hex for '${_case_id}'")
            endif()
        elseif(NOT _pointer_hex STREQUAL "-")
            message(FATAL_ERROR "Unexpected pointer bytes for '${_case_id}'")
        endif()
        if(_offset_assertion STREQUAL "EXACT")
            if(NOT _offset_value MATCHES "^(0|[1-9][0-9]*)$" OR
               NOT _offset_kind STREQUAL "EXACT_INPUT_BYTE")
                message(FATAL_ERROR "Invalid exact offset for '${_case_id}'")
            endif()
            if(NOT _offset_value LESS _expected_size)
                message(FATAL_ERROR "Exact offset is outside the input for '${_case_id}'")
            endif()
        elseif(_offset_assertion STREQUAL "KIND_ONLY")
            if(NOT _offset_value STREQUAL "-" OR _offset_kind STREQUAL "NONE")
                message(FATAL_ERROR "Invalid kind-only offset for '${_case_id}'")
            endif()
        elseif(NOT _offset_value STREQUAL "-" OR NOT _offset_kind STREQUAL "NONE")
            message(FATAL_ERROR "ABSENT/IGNORE offset must use NONE and '-' for '${_case_id}'")
        endif()
        if(_offset_kind STREQUAL "PARSER_REPORTED_POSITION" AND
           NOT _offset_assertion STREQUAL "KIND_ONLY")
            message(FATAL_ERROR
                "Parser-reported offsets may only use KIND_ONLY for '${_case_id}'")
        endif()

        string(APPEND PAE_STRICT_JSON_FIXTURE_RECORDS
            "  {\"${_case_id}\", \"${_rule_id}\", \"${_suite}\", \"${_status}\", "
            "\"${_input_hex}\", \"${_limits_id}\", \"${_expected_code}\", "
            "\"${_target_code}\", \"${_target_lock_candidate}\", "
            "\"${_pointer_assertion}\", \"${_pointer_hex}\", "
            "\"${_offset_assertion}\", \"${_offset_kind}\", \"${_offset_value}\", "
            "${_expected_size}ULL},\n")
    endforeach()
    list(LENGTH _case_ids _strict_case_count)
    list(LENGTH _manifest_authority_ids _manifest_authority_count)
    if(NOT _strict_case_count EQUAL _manifest_authority_count)
        message(FATAL_ERROR
            "Strict corpus and MANIFEST_AUTHORITY inventory counts differ: "
            "${_strict_case_count} != ${_manifest_authority_count}")
    endif()

    pae_read_lf_tsv(
        "${_number_inventory_path}"
        "case_id"
        _number_inventory_lines
    )
    set(_number_inventory_case_ids)
    foreach(_line IN LISTS _number_inventory_lines)
        if(NOT _line MATCHES "^[a-z][a-z0-9_]*$" OR _line IN_LIST _number_inventory_case_ids)
            message(FATAL_ERROR "Invalid or duplicate Number Token inventory case_id '${_line}'")
        endif()
        list(APPEND _number_inventory_case_ids "${_line}")
    endforeach()

    pae_read_lf_tsv(
        "${_number_path}"
        "case_id\ttarget_type\tstatus\ttoken_hex\texpected_kind\texpected_negative_zero\texpected_status\texpected_reason\texpected_value\texpected_bits_hex"
        _number_lines
    )
    set(_valid_number_targets UINT64 INT64 REAL64)
    set(_valid_number_statuses CONFORMANCE OPEN_DECISION)
    set(_valid_number_kinds UNSIGNED_INTEGER SIGNED_INTEGER REAL INVALID)
    set(_valid_booleans true false)
    set(_valid_integer_results OK NOT_INTEGER OUT_OF_RANGE INVALID_TOKEN)
    set(_valid_real_results OK OVERFLOW UNDERFLOW INVALID_TOKEN)
    set(_valid_number_reasons
        NONE NEGATIVE_TOKEN_FOR_UNSIGNED NOT_INTEGER INTEGER_OUT_OF_RANGE INVALID_TOKEN
        OVERFLOW UNDERFLOW
    )
    set(_number_case_ids)
    set(PAE_NUMBER_TOKEN_RECORDS "")
    foreach(_line IN LISTS _number_lines)
        string(REPLACE "\t" ";" _fields "${_line}")
        list(LENGTH _fields _field_count)
        if(NOT _field_count EQUAL 10)
            message(FATAL_ERROR "Number token corpus row must contain 10 fields: ${_line}")
        endif()
        list(GET _fields 0 _case_id)
        list(GET _fields 1 _target_type)
        list(GET _fields 2 _status)
        list(GET _fields 3 _token_hex)
        list(GET _fields 4 _expected_kind)
        list(GET _fields 5 _expected_negative_zero)
        list(GET _fields 6 _expected_status)
        list(GET _fields 7 _expected_reason)
        list(GET _fields 8 _expected_value)
        list(GET _fields 9 _expected_bits_hex)
        if(NOT _case_id MATCHES "^[a-z][a-z0-9_]*$" OR _case_id IN_LIST _number_case_ids)
            message(FATAL_ERROR "Invalid or duplicate number case_id '${_case_id}'")
        endif()
        if(NOT _case_id IN_LIST _number_inventory_case_ids)
            message(FATAL_ERROR "Number Token case '${_case_id}' is missing from its inventory")
        endif()
        list(APPEND _number_case_ids "${_case_id}")
        pae_require_member("${_target_type}" "${_valid_number_targets}"
                           "number target for ${_case_id}")
        pae_require_member("${_status}" "${_valid_number_statuses}"
                           "number status for ${_case_id}")
        pae_require_member("${_expected_kind}" "${_valid_number_kinds}"
                           "number kind for ${_case_id}")
        pae_require_member("${_expected_negative_zero}" "${_valid_booleans}"
                           "negative zero flag for ${_case_id}")
        if(_target_type STREQUAL "REAL64")
            pae_require_member("${_expected_status}" "${_valid_real_results}"
                               "REAL64 result for ${_case_id}")
        else()
            pae_require_member("${_expected_status}" "${_valid_integer_results}"
                               "integer result for ${_case_id}")
        endif()
        pae_require_member("${_expected_reason}" "${_valid_number_reasons}"
                           "number reason for ${_case_id}")
        if(NOT _token_hex STREQUAL "-" AND
           NOT _token_hex MATCHES "^([0-9A-F][0-9A-F])+$")
            message(FATAL_ERROR "Invalid token hex for '${_case_id}'")
        endif()
        if(_target_type STREQUAL "REAL64")
            if(NOT _expected_value STREQUAL "-")
                message(FATAL_ERROR "REAL64 must use bit-pattern assertions for '${_case_id}'")
            endif()
            if(_expected_status STREQUAL "OK")
                string(LENGTH "${_expected_bits_hex}" _expected_bits_length)
                if(NOT _expected_bits_length EQUAL 16 OR
                   NOT _expected_bits_hex MATCHES "^[0-9A-F]+$")
                    message(FATAL_ERROR "Missing REAL64 bit pattern for '${_case_id}'")
                endif()
            elseif(NOT _expected_bits_hex STREQUAL "-")
                message(FATAL_ERROR "Unexpected REAL64 bit pattern for '${_case_id}'")
            endif()
        elseif(_expected_status STREQUAL "OK")
            if(NOT _expected_value MATCHES "^(0|-?[1-9][0-9]*)$" OR
               NOT _expected_bits_hex STREQUAL "-")
                message(FATAL_ERROR "Missing exact integer value for '${_case_id}'")
            endif()
        elseif(NOT _expected_value STREQUAL "-" OR NOT _expected_bits_hex STREQUAL "-")
            message(FATAL_ERROR "Unexpected exact value for '${_case_id}'")
        endif()
        if(_expected_status STREQUAL "OK")
            if(NOT _expected_reason STREQUAL "NONE")
                message(FATAL_ERROR "Successful number case must use NONE reason: ${_case_id}")
            endif()
        elseif(_expected_status STREQUAL "NOT_INTEGER")
            if(NOT _expected_reason STREQUAL "NOT_INTEGER")
                message(FATAL_ERROR "NOT_INTEGER reason mismatch for '${_case_id}'")
            endif()
        elseif(_expected_status STREQUAL "OUT_OF_RANGE")
            set(_valid_range_reasons NEGATIVE_TOKEN_FOR_UNSIGNED INTEGER_OUT_OF_RANGE)
            pae_require_member("${_expected_reason}" "${_valid_range_reasons}"
                               "integer range reason for ${_case_id}")
        elseif(_expected_status STREQUAL "OVERFLOW")
            if(NOT _expected_reason STREQUAL "OVERFLOW")
                message(FATAL_ERROR "REAL64 overflow reason mismatch for '${_case_id}'")
            endif()
        elseif(_expected_status STREQUAL "UNDERFLOW")
            if(NOT _expected_reason STREQUAL "UNDERFLOW")
                message(FATAL_ERROR "REAL64 underflow reason mismatch for '${_case_id}'")
            endif()
        elseif(NOT _expected_reason STREQUAL "INVALID_TOKEN")
            message(FATAL_ERROR "Invalid-token reason mismatch for '${_case_id}'")
        endif()
        string(APPEND PAE_NUMBER_TOKEN_RECORDS
            "  {\"${_case_id}\", \"${_target_type}\", \"${_status}\", "
            "\"${_token_hex}\", \"${_expected_kind}\", \"${_expected_negative_zero}\", "
            "\"${_expected_status}\", \"${_expected_reason}\", \"${_expected_value}\", "
            "\"${_expected_bits_hex}\"},\n")
    endforeach()
    list(LENGTH _number_case_ids _number_case_count)
    list(LENGTH _number_inventory_case_ids _number_inventory_case_count)
    if(NOT _number_case_count EQUAL _number_inventory_case_count)
        message(FATAL_ERROR
            "Number Token corpus and inventory counts differ: "
            "${_number_case_count} != ${_number_inventory_case_count}")
    endif()

    file(MAKE_DIRECTORY "${output_dir}")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/strict_json_corpus_generated.h.in"
        "${output_dir}/strict_json_corpus_generated.h"
        @ONLY
        NEWLINE_STYLE LF
    )
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/number_token_corpus_generated.h.in"
        "${output_dir}/number_token_corpus_generated.h"
        @ONLY
        NEWLINE_STYLE LF
    )
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/resource_profiles_generated.h.in"
        "${output_dir}/resource_profiles_generated.h"
        @ONLY
        NEWLINE_STYLE LF
    )
endfunction()
