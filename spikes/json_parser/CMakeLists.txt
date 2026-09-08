include(FetchContent)

include(cmake/GenerateCorpus.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake)
set(_pae_json_spike_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
pae_generate_json_spike_corpora("${_pae_json_spike_generated_dir}")

set(PAE_JSON_SPIKE_CANDIDATE "all" CACHE STRING "JSON parser spike candidate: all, yyjson, nlohmann, or rapidjson")
set_property(CACHE PAE_JSON_SPIKE_CANDIDATE PROPERTY STRINGS all yyjson nlohmann rapidjson)

set(_pae_valid_candidates all yyjson nlohmann rapidjson)
if(NOT PAE_JSON_SPIKE_CANDIDATE IN_LIST _pae_valid_candidates)
    message(FATAL_ERROR "Unsupported PAE_JSON_SPIKE_CANDIDATE='${PAE_JSON_SPIKE_CANDIDATE}'")
endif()

if(PAE_BUILD_TESTING)
    function(pae_add_json_corpus_rejection_test mutation expected_diagnostic)
        add_test(
            NAME pae_json_corpus_reject_${mutation}
            COMMAND
                "${CMAKE_COMMAND}"
                "-DPAE_JSON_SPIKE_SOURCE_FIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                "-DPAE_JSON_SPIKE_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/negative_corpus/${mutation}"
                "-DPAE_JSON_SPIKE_MUTATION=${mutation}"
                "-DPAE_JSON_SPIKE_EXPECTED_DIAGNOSTIC=${expected_diagnostic}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyCorpusRejection.cmake"
        )
        set_tests_properties(pae_json_corpus_reject_${mutation} PROPERTIES TIMEOUT 10)
    endfunction()

    pae_add_json_corpus_rejection_test(exact_parser_reported "Invalid exact offset")
    pae_add_json_corpus_rejection_test(exact_outside_input "Exact offset is outside the input")
    pae_add_json_corpus_rejection_test(kind_only_none "Invalid kind-only offset")
    pae_add_json_corpus_rejection_test(
        non_characterization_target
        "Only characterization cases may define target_code or target lock"
    )
    pae_add_json_corpus_rejection_test(
        missing_inventory
        "'negative_missing_inventory' is missing"
    )
endif()

function(pae_add_json_spike_executable target source)
    add_executable(
        ${target}
        ${source}
        src/spike_common.cpp
        src/number_token.cpp
    )
    target_include_directories(${target} PRIVATE include "${_pae_json_spike_generated_dir}")
    target_link_libraries(${target} PRIVATE pae::project_options)

    if(PAE_BUILD_TESTING)
        add_test(NAME ${target} COMMAND ${target} --benchmark-iterations 1000)
        set_tests_properties(${target} PROPERTIES TIMEOUT 60)
        add_test(NAME ${target}_reject_negative_iterations COMMAND ${target} --benchmark-iterations -1)
        set_tests_properties(
            ${target}_reject_negative_iterations
            PROPERTIES
                TIMEOUT 5
                WILL_FAIL TRUE
        )
        add_test(NAME ${target}_reject_excessive_iterations COMMAND ${target} --benchmark-iterations 10000001)
        set_tests_properties(
            ${target}_reject_excessive_iterations
            PROPERTIES
                TIMEOUT 5
                WILL_FAIL TRUE
        )
    endif()
endfunction()

add_executable(
    pae_json_number_token_corpus
    src/number_token.cpp
    src/number_token_corpus.cpp
)
target_include_directories(
    pae_json_number_token_corpus
    PRIVATE
        include
        "${_pae_json_spike_generated_dir}"
)
target_link_libraries(pae_json_number_token_corpus PRIVATE pae::project_options)
if(PAE_BUILD_TESTING)
    add_test(NAME pae_json_number_token_corpus COMMAND pae_json_number_token_corpus)
    set_tests_properties(pae_json_number_token_corpus PROPERTIES TIMEOUT 5)
endif()

if(PAE_JSON_SPIKE_CANDIDATE STREQUAL "all" OR PAE_JSON_SPIKE_CANDIDATE STREQUAL "yyjson")
    pae_add_yyjson()

    pae_add_json_spike_executable(pae_json_spike_yyjson src/yyjson_adapter.cpp)
    target_link_libraries(pae_json_spike_yyjson PRIVATE pae::yyjson)
    if(PAE_BUILD_TESTING)
        add_test(
            NAME pae_json_spike_yyjson_allocator_limits
            COMMAND pae_json_spike_yyjson --allocator-self-test
        )
        set_tests_properties(pae_json_spike_yyjson_allocator_limits PROPERTIES TIMEOUT 5)
        add_test(
            NAME pae_json_spike_yyjson_number_contract
            COMMAND pae_json_spike_yyjson --number-contract-self-test
        )
        set_tests_properties(pae_json_spike_yyjson_number_contract PROPERTIES TIMEOUT 10)
        foreach(_pae_resource_profile constrained desktop hard)
            add_test(
                NAME pae_json_spike_yyjson_resource_${_pae_resource_profile}
                COMMAND
                    pae_json_spike_yyjson
                    --resource-profile-gate
                    ${_pae_resource_profile}
            )
            set_tests_properties(
                pae_json_spike_yyjson_resource_${_pae_resource_profile}
                PROPERTIES
                    LABELS "windows_parser_stage_resource_profile"
                    RUN_SERIAL TRUE
                    TIMEOUT 180
            )
        endforeach()
    endif()
endif()

if(PAE_JSON_SPIKE_CANDIDATE STREQUAL "all" OR PAE_JSON_SPIKE_CANDIDATE STREQUAL "nlohmann")
    FetchContent_Declare(
        pae_spike_nlohmann_source
        URL https://codeload.github.com/nlohmann/json/tar.gz/65ee68451d8eb2b5f3a30b410476ab83deb3289b
        URL_HASH SHA256=13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _pae_no_upstream_cmake
    )
    FetchContent_MakeAvailable(pae_spike_nlohmann_source)

    pae_add_json_spike_executable(pae_json_spike_nlohmann src/nlohmann_adapter.cpp)
    target_include_directories(
        pae_json_spike_nlohmann
        SYSTEM PRIVATE
            ${pae_spike_nlohmann_source_SOURCE_DIR}/single_include
    )
endif()

if(PAE_JSON_SPIKE_CANDIDATE STREQUAL "all" OR PAE_JSON_SPIKE_CANDIDATE STREQUAL "rapidjson")
    FetchContent_Declare(
        pae_spike_rapidjson_source
        URL https://codeload.github.com/Tencent/rapidjson/tar.gz/f54b0e47a08782a6131cc3d60f94d038fa6e0a51
        URL_HASH SHA256=4a76453d36770c9628d7d175a2e9baccbfbd2169ced44f0cb72e86c5f5f2f7cd
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _pae_no_upstream_cmake
    )
    FetchContent_MakeAvailable(pae_spike_rapidjson_source)

    pae_add_json_spike_executable(pae_json_spike_rapidjson src/rapidjson_adapter.cpp)
    target_include_directories(
        pae_json_spike_rapidjson
        SYSTEM PRIVATE
            ${pae_spike_rapidjson_source_SOURCE_DIR}/include
    )
    target_compile_definitions(pae_json_spike_rapidjson PRIVATE RAPIDJSON_HAS_STDSTRING=1)
endif()
