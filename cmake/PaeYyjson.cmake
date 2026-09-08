include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/VerifyYyjsonVendor.cmake")

function(pae_add_yyjson)
    set(_pae_yyjson_root "${PROJECT_SOURCE_DIR}/third_party/yyjson")
    set(_pae_yyjson_lock "${_pae_yyjson_root}/dependency.lock.json")
    pae_verify_yyjson_vendor("${_pae_yyjson_lock}" "${_pae_yyjson_root}")
    pae_read_yyjson_vendor_lock("${_pae_yyjson_lock}" _pae_yyjson_version _pae_yyjson_commit)

    if(TARGET pae_yyjson_vendor)
        get_target_property(_pae_existing_source_dir pae_yyjson_vendor PAE_YYJSON_SOURCE_DIR)
        get_target_property(_pae_existing_version pae_yyjson_vendor PAE_YYJSON_VERSION)
        get_target_property(_pae_existing_commit pae_yyjson_vendor PAE_YYJSON_COMMIT)
        if(NOT _pae_existing_source_dir STREQUAL _pae_yyjson_root OR
           NOT _pae_existing_version STREQUAL _pae_yyjson_version OR
           NOT _pae_existing_commit STREQUAL _pae_yyjson_commit)
            message(FATAL_ERROR "Conflicting yyjson vendor definitions requested by PAE build consumers")
        endif()
        return()
    endif()

    add_library(pae_yyjson_vendor STATIC "${_pae_yyjson_root}/src/yyjson.c")
    set_target_properties(
        pae_yyjson_vendor
        PROPERTIES
            PAE_YYJSON_SOURCE_DIR "${_pae_yyjson_root}"
            PAE_YYJSON_VERSION "${_pae_yyjson_version}"
            PAE_YYJSON_COMMIT "${_pae_yyjson_commit}"
    )
    add_library(pae::yyjson ALIAS pae_yyjson_vendor)
    target_include_directories(
        pae_yyjson_vendor
        SYSTEM PUBLIC
            "${_pae_yyjson_root}/src"
    )
    target_compile_definitions(
        pae_yyjson_vendor
        PUBLIC
            YYJSON_DISABLE_INCR_READER=1
            YYJSON_DISABLE_NON_STANDARD=1
            YYJSON_DISABLE_UTILS=1
            YYJSON_DISABLE_WRITER=1
    )
endfunction()
