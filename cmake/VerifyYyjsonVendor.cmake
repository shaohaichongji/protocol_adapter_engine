include_guard(GLOBAL)

function(pae_read_yyjson_vendor_lock lock_file output_version output_commit)
    if(NOT EXISTS "${lock_file}")
        message(FATAL_ERROR "yyjson dependency lock does not exist: ${lock_file}")
    endif()

    file(READ "${lock_file}" _lock_json)
    string(JSON _schema_version GET "${_lock_json}" schema_version)
    if(NOT _schema_version EQUAL 1)
        message(FATAL_ERROR "Unsupported yyjson dependency lock schema_version '${_schema_version}'")
    endif()

    string(JSON _name GET "${_lock_json}" dependency name)
    string(JSON _version GET "${_lock_json}" dependency version)
    string(JSON _commit GET "${_lock_json}" dependency commit)
    string(JSON _license GET "${_lock_json}" dependency license)
    string(JSON _vendored_root GET "${_lock_json}" dependency vendored_root)
    string(JSON _local_modifications GET "${_lock_json}" dependency local_modifications)
    string(JSON _selection_status GET "${_lock_json}" dependency selection_status)

    if(NOT _name STREQUAL "yyjson")
        message(FATAL_ERROR "Unexpected vendored dependency '${_name}'")
    endif()
    if(NOT _version STREQUAL "0.12.0" OR
       NOT _commit STREQUAL "8b4a38dc994a110abaec8a400615567bd996105f")
        message(FATAL_ERROR "Unexpected adopted yyjson version or commit")
    endif()
    if(NOT _license STREQUAL "MIT")
        message(FATAL_ERROR "Unexpected yyjson license '${_license}'")
    endif()
    if(NOT _vendored_root STREQUAL "third_party/yyjson")
        message(FATAL_ERROR "Unexpected yyjson vendored_root '${_vendored_root}'")
    endif()
    if(NOT _local_modifications STREQUAL "none")
        message(FATAL_ERROR "Vendored yyjson must remain byte-identical to the locked upstream files")
    endif()
    if(NOT _selection_status STREQUAL "adopted")
        message(FATAL_ERROR "yyjson dependency lock must record selection_status 'adopted'")
    endif()

    set(${output_version} "${_version}" PARENT_SCOPE)
    set(${output_commit} "${_commit}" PARENT_SCOPE)
endfunction()

function(pae_verify_locked_file root relative_path expected_bytes expected_sha256 description)
    string(FIND "${relative_path}" "\\" _backslash_index)
    string(LENGTH "${expected_sha256}" _sha256_length)
    if(relative_path STREQUAL "" OR IS_ABSOLUTE "${relative_path}" OR
       NOT _backslash_index EQUAL -1 OR
       relative_path MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "Unsafe yyjson ${description} path '${relative_path}'")
    endif()
    if(NOT _sha256_length EQUAL 64 OR NOT expected_sha256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "Invalid yyjson ${description} SHA-256 in dependency lock")
    endif()

    set(_path "${root}/${relative_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Locked yyjson ${description} file is missing: ${relative_path}")
    endif()
    file(SIZE "${_path}" _actual_bytes)
    file(SHA256 "${_path}" _actual_sha256)
    string(TOLOWER "${_actual_sha256}" _actual_sha256)
    if(NOT _actual_bytes EQUAL expected_bytes OR
       NOT _actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Locked yyjson ${description} file changed: ${relative_path} "
            "(bytes ${_actual_bytes}/${expected_bytes}, "
            "sha256 ${_actual_sha256}/${expected_sha256})"
        )
    endif()
endfunction()

function(pae_verify_yyjson_vendor lock_file vendor_root)
    pae_read_yyjson_vendor_lock("${lock_file}" _version _commit)
    file(READ "${lock_file}" _lock_json)

    string(JSON _source_count LENGTH "${_lock_json}" dependency source_scope)
    if(NOT _source_count EQUAL 2)
        message(FATAL_ERROR "yyjson source_scope must contain exactly two files")
    endif()
    set(_seen_header FALSE)
    set(_seen_source FALSE)
    math(EXPR _last_source_index "${_source_count} - 1")
    foreach(_source_index RANGE 0 ${_last_source_index})
        string(JSON _relative_path GET "${_lock_json}" dependency source_scope ${_source_index} path)
        string(JSON _expected_bytes GET "${_lock_json}" dependency source_scope ${_source_index} bytes)
        string(JSON _expected_sha256 GET "${_lock_json}" dependency source_scope ${_source_index} sha256)
        if(_relative_path STREQUAL "src/yyjson.h")
            if(_seen_header)
                message(FATAL_ERROR "yyjson source_scope contains duplicate src/yyjson.h")
            endif()
            set(_seen_header TRUE)
        elseif(_relative_path STREQUAL "src/yyjson.c")
            if(_seen_source)
                message(FATAL_ERROR "yyjson source_scope contains duplicate src/yyjson.c")
            endif()
            set(_seen_source TRUE)
        else()
            message(FATAL_ERROR "Unexpected yyjson source_scope path '${_relative_path}'")
        endif()
        pae_verify_locked_file(
            "${vendor_root}"
            "${_relative_path}"
            "${_expected_bytes}"
            "${_expected_sha256}"
            "source"
        )
    endforeach()
    if(NOT _seen_header OR NOT _seen_source)
        message(FATAL_ERROR "yyjson source_scope must contain src/yyjson.h and src/yyjson.c")
    endif()

    string(JSON _license_path GET "${_lock_json}" dependency license_file path)
    string(JSON _license_bytes GET "${_lock_json}" dependency license_file bytes)
    string(JSON _license_sha256 GET "${_lock_json}" dependency license_file sha256)
    if(NOT _license_path STREQUAL "LICENSE")
        message(FATAL_ERROR "Unexpected yyjson license path '${_license_path}'")
    endif()
    pae_verify_locked_file(
        "${vendor_root}"
        "${_license_path}"
        "${_license_bytes}"
        "${_license_sha256}"
        "license"
    )
endfunction()
