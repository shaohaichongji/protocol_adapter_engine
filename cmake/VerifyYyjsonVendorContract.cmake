if(NOT DEFINED PAE_SOURCE_DIR OR NOT DEFINED PAE_BINARY_DIR)
    message(FATAL_ERROR "PAE_SOURCE_DIR and PAE_BINARY_DIR are required")
endif()

set(_verify_module "${PAE_SOURCE_DIR}/cmake/VerifyYyjsonVendor.cmake")
set(_worker "${PAE_SOURCE_DIR}/cmake/VerifyYyjsonVendorFixture.cmake")
set(_vendor_root "${PAE_SOURCE_DIR}/third_party/yyjson")
set(_lock "${_vendor_root}/dependency.lock.json")
include("${_verify_module}")
pae_verify_yyjson_vendor("${_lock}" "${_vendor_root}")

file(STRINGS "${PAE_SOURCE_DIR}/.gitattributes" _attributes)
foreach(
    _expected_attribute
    IN ITEMS
        "third_party/yyjson/LICENSE -text"
        "third_party/yyjson/src/yyjson.h -text"
        "third_party/yyjson/src/yyjson.c -text"
)
    list(FIND _attributes "${_expected_attribute}" _attribute_index)
    if(_attribute_index EQUAL -1)
        message(FATAL_ERROR "Missing locked yyjson Git attribute: ${_expected_attribute}")
    endif()
endforeach()
message(STATUS "yyjson Git attributes disable text conversion for every locked upstream file")

set(_work "${PAE_BINARY_DIR}/yyjson_vendor_contract")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}")

function(pae_expect_yyjson_rejection case_name lock root expected_pattern)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPAE_YYJSON_VERIFY_MODULE=${_verify_module}"
            "-DPAE_YYJSON_LOCK=${lock}"
            "-DPAE_YYJSON_ROOT=${root}"
            -P "${_worker}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    set(_combined "${_stdout}\n${_stderr}")
    if(_result EQUAL 0)
        message(FATAL_ERROR "${case_name}: invalid yyjson fixture was accepted")
    endif()
    if(NOT _combined MATCHES "${expected_pattern}")
        message(FATAL_ERROR "${case_name}: unexpected diagnostic:\n${_combined}")
    endif()
    message(STATUS "${case_name}: rejected with '${expected_pattern}'")
endfunction()

pae_expect_yyjson_rejection(
    missing_lock
    "${_work}/missing.lock.json"
    "${_vendor_root}"
    "yyjson dependency lock does not exist"
)

file(COPY "${_vendor_root}/" DESTINATION "${_work}/missing_source")
file(REMOVE "${_work}/missing_source/src/yyjson.c")
pae_expect_yyjson_rejection(
    missing_source
    "${_work}/missing_source/dependency.lock.json"
    "${_work}/missing_source"
    "Locked yyjson source file is missing: src/yyjson.c"
)

file(COPY "${_vendor_root}/" DESTINATION "${_work}/changed_source")
file(APPEND "${_work}/changed_source/src/yyjson.h" "changed")
pae_expect_yyjson_rejection(
    changed_source
    "${_work}/changed_source/dependency.lock.json"
    "${_work}/changed_source"
    "Locked yyjson source file changed: src/yyjson.h"
)

message(STATUS "yyjson vendor contract positive and negative checks passed")
