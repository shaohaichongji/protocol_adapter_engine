include_guard(GLOBAL)

function(pae_find_yyjson_candidate lock_json output_index)
    string(JSON _candidate_count LENGTH "${lock_json}" candidates)
    if(_candidate_count EQUAL 0)
        message(FATAL_ERROR "Dependency lock contains no candidates")
    endif()

    math(EXPR _last_candidate_index "${_candidate_count} - 1")
    set(_yyjson_index -1)
    foreach(_candidate_index RANGE 0 ${_last_candidate_index})
        string(JSON _candidate_name GET "${lock_json}" candidates ${_candidate_index} name)
        if(_candidate_name STREQUAL "yyjson")
            if(NOT _yyjson_index EQUAL -1)
                message(FATAL_ERROR "Dependency lock contains duplicate yyjson candidates")
            endif()
            set(_yyjson_index ${_candidate_index})
        endif()
    endforeach()
    if(_yyjson_index EQUAL -1)
        message(FATAL_ERROR "Dependency lock does not contain yyjson")
    endif()
    set(${output_index} ${_yyjson_index} PARENT_SCOPE)
endfunction()

function(pae_read_yyjson_candidate_lock lock_file output_url output_sha256)
    if(NOT EXISTS "${lock_file}")
        message(FATAL_ERROR "Dependency lock does not exist: ${lock_file}")
    endif()
    file(READ "${lock_file}" _lock_json)
    string(JSON _schema_version GET "${_lock_json}" schema_version)
    if(NOT _schema_version EQUAL 1)
        message(FATAL_ERROR "Unsupported dependency lock schema_version '${_schema_version}'")
    endif()

    pae_find_yyjson_candidate("${_lock_json}" _yyjson_index)
    string(JSON _selection_status
        GET "${_lock_json}" candidates ${_yyjson_index} selection_status)
    string(JSON _local_modifications
        GET "${_lock_json}" candidates ${_yyjson_index} local_modifications)
    string(JSON _license GET "${_lock_json}" candidates ${_yyjson_index} license)
    if(NOT _selection_status STREQUAL "candidate")
        message(FATAL_ERROR
            "yyjson must remain a candidate until the production Parser decision is frozen")
    endif()
    if(NOT _local_modifications STREQUAL "none")
        message(FATAL_ERROR "The yyjson Spike only accepts an unmodified upstream source scope")
    endif()
    if(NOT _license STREQUAL "MIT")
        message(FATAL_ERROR "Unexpected yyjson license '${_license}'")
    endif()

    string(JSON _archive_url GET "${_lock_json}" candidates ${_yyjson_index} archive_url)
    string(JSON _archive_sha256
        GET "${_lock_json}" candidates ${_yyjson_index} archive_sha256)
    string(LENGTH "${_archive_sha256}" _archive_sha256_length)
    if(NOT _archive_sha256_length EQUAL 64 OR
       NOT _archive_sha256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "Invalid yyjson archive SHA-256 in the dependency lock")
    endif()
    set(${output_url} "${_archive_url}" PARENT_SCOPE)
    set(${output_sha256} "${_archive_sha256}" PARENT_SCOPE)
endfunction()

function(pae_verify_yyjson_candidate_source lock_file source_dir)
    file(READ "${lock_file}" _lock_json)
    pae_find_yyjson_candidate("${_lock_json}" _yyjson_index)

    string(JSON _source_count LENGTH
        "${_lock_json}" candidates ${_yyjson_index} candidate_source_scope)
    if(NOT _source_count EQUAL 2)
        message(FATAL_ERROR "yyjson candidate source scope must contain exactly two files")
    endif()

    math(EXPR _last_source_index "${_source_count} - 1")
    foreach(_source_index RANGE 0 ${_last_source_index})
        string(JSON _relative_path GET
            "${_lock_json}" candidates ${_yyjson_index} candidate_source_scope
            ${_source_index} path)
        string(JSON _expected_bytes GET
            "${_lock_json}" candidates ${_yyjson_index} candidate_source_scope
            ${_source_index} bytes)
        string(JSON _expected_sha256 GET
            "${_lock_json}" candidates ${_yyjson_index} candidate_source_scope
            ${_source_index} sha256)
        string(FIND "${_relative_path}" "\\" _backslash_index)
        if(_relative_path STREQUAL "" OR IS_ABSOLUTE "${_relative_path}" OR
           NOT _backslash_index EQUAL -1 OR
           _relative_path MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR "Unsafe yyjson source path '${_relative_path}'")
        endif()
        set(_source_path "${source_dir}/${_relative_path}")
        if(NOT EXISTS "${_source_path}")
            message(FATAL_ERROR "Locked yyjson source file is missing: ${_relative_path}")
        endif()
        file(SIZE "${_source_path}" _actual_bytes)
        file(SHA256 "${_source_path}" _actual_sha256)
        string(TOLOWER "${_actual_sha256}" _actual_sha256)
        if(NOT _actual_bytes EQUAL _expected_bytes OR
           NOT _actual_sha256 STREQUAL _expected_sha256)
            message(FATAL_ERROR
                "Locked yyjson source file changed: ${_relative_path} "
                "(bytes ${_actual_bytes}/${_expected_bytes}, "
                "sha256 ${_actual_sha256}/${_expected_sha256})")
        endif()
    endforeach()

    string(JSON _license_path GET
        "${_lock_json}" candidates ${_yyjson_index} license_file path)
    string(JSON _license_bytes GET
        "${_lock_json}" candidates ${_yyjson_index} license_file bytes)
    string(JSON _license_sha256 GET
        "${_lock_json}" candidates ${_yyjson_index} license_file sha256)
    string(FIND "${_license_path}" "\\" _license_backslash_index)
    if(_license_path STREQUAL "" OR IS_ABSOLUTE "${_license_path}" OR
       NOT _license_backslash_index EQUAL -1 OR
       _license_path MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "Unsafe yyjson license path '${_license_path}'")
    endif()
    set(_license_file "${source_dir}/${_license_path}")
    if(NOT EXISTS "${_license_file}")
        message(FATAL_ERROR "Locked yyjson license file is missing")
    endif()
    file(SIZE "${_license_file}" _actual_license_bytes)
    file(SHA256 "${_license_file}" _actual_license_sha256)
    string(TOLOWER "${_actual_license_sha256}" _actual_license_sha256)
    if(NOT _actual_license_bytes EQUAL _license_bytes OR
       NOT _actual_license_sha256 STREQUAL _license_sha256)
        message(FATAL_ERROR
            "Locked yyjson license changed "
            "(bytes ${_actual_license_bytes}/${_license_bytes}, "
            "sha256 ${_actual_license_sha256}/${_license_sha256})")
    endif()
endfunction()
