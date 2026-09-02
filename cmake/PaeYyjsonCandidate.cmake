include_guard(GLOBAL)

include(FetchContent)

include("${CMAKE_CURRENT_LIST_DIR}/VerifyYyjsonCandidate.cmake")

function(pae_add_yyjson_candidate lock_file)
    pae_read_yyjson_candidate_lock(
        "${lock_file}"
        _pae_yyjson_archive_url
        _pae_yyjson_archive_sha256
    )

    if(TARGET pae_yyjson_candidate_vendor)
        get_target_property(
            _pae_existing_archive_url
            pae_yyjson_candidate_vendor
            PAE_YYJSON_ARCHIVE_URL
        )
        get_target_property(
            _pae_existing_archive_sha256
            pae_yyjson_candidate_vendor
            PAE_YYJSON_ARCHIVE_SHA256
        )
        get_target_property(
            _pae_existing_source_dir
            pae_yyjson_candidate_vendor
            PAE_YYJSON_SOURCE_DIR
        )
        if(
            NOT _pae_existing_archive_url STREQUAL _pae_yyjson_archive_url
            OR NOT _pae_existing_archive_sha256 STREQUAL _pae_yyjson_archive_sha256
        )
            message(
                FATAL_ERROR
                "Conflicting yyjson candidate locks requested by PAE build consumers"
            )
        endif()
        pae_verify_yyjson_candidate_source(
            "${lock_file}"
            "${_pae_existing_source_dir}"
        )
        return()
    endif()

    FetchContent_Declare(
        pae_yyjson_candidate_source
        URL "${_pae_yyjson_archive_url}"
        URL_HASH "SHA256=${_pae_yyjson_archive_sha256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _pae_no_upstream_cmake
    )
    FetchContent_MakeAvailable(pae_yyjson_candidate_source)
    pae_verify_yyjson_candidate_source(
        "${lock_file}"
        "${pae_yyjson_candidate_source_SOURCE_DIR}"
    )

    add_library(
        pae_yyjson_candidate_vendor
        STATIC
        "${pae_yyjson_candidate_source_SOURCE_DIR}/src/yyjson.c"
    )
    set_target_properties(
        pae_yyjson_candidate_vendor
        PROPERTIES
            PAE_YYJSON_ARCHIVE_URL "${_pae_yyjson_archive_url}"
            PAE_YYJSON_ARCHIVE_SHA256 "${_pae_yyjson_archive_sha256}"
            PAE_YYJSON_SOURCE_DIR "${pae_yyjson_candidate_source_SOURCE_DIR}"
    )
    add_library(pae::yyjson_candidate ALIAS pae_yyjson_candidate_vendor)
    target_include_directories(
        pae_yyjson_candidate_vendor
        SYSTEM PUBLIC
            "${pae_yyjson_candidate_source_SOURCE_DIR}/src"
    )
    target_compile_definitions(
        pae_yyjson_candidate_vendor
        PUBLIC
            YYJSON_DISABLE_INCR_READER=1
            YYJSON_DISABLE_NON_STANDARD=1
            YYJSON_DISABLE_UTILS=1
            YYJSON_DISABLE_WRITER=1
    )
endfunction()
