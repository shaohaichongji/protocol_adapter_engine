add_executable(
    pae_config_compiler_contract_tests
    config_compiler_contract_tests.cpp
    plan_builder_test_peer.cpp
    plan_builder_test_peer.h
)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

function(pae_expect_capability_compile case_name source_name should_compile)
    set(result_variable "pae_capability_${case_name}_compiled")
    unset(${result_variable} CACHE)
    try_compile(
        ${result_variable}
        "${CMAKE_CURRENT_BINARY_DIR}/compile_contracts/${case_name}"
        "${CMAKE_CURRENT_SOURCE_DIR}/compile_contracts/${source_name}"
        CMAKE_FLAGS
            "-DCMAKE_CXX_STANDARD=17"
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
            "-DCMAKE_CXX_EXTENSIONS=OFF"
        OUTPUT_VARIABLE compile_output
    )
    if(should_compile AND NOT ${result_variable})
        message(
            FATAL_ERROR
            "Capability positive compile contract '${case_name}' failed:\n${compile_output}"
        )
    endif()
    if(NOT should_compile AND ${result_variable})
        message(
            FATAL_ERROR
            "Capability bypass compile contract '${case_name}' unexpectedly compiled."
        )
    endif()
    message(
        STATUS
        "PAE capability compile contract ${case_name}: "
        "expected=${should_compile} actual=${${result_variable}}"
    )
endfunction()

pae_expect_capability_compile(positive capability_positive.cpp TRUE)
pae_expect_capability_compile(validated_default validated_default_forbidden.cpp FALSE)
pae_expect_capability_compile(budgeted_default budgeted_default_forbidden.cpp FALSE)
pae_expect_capability_compile(plan_draft_default plan_draft_default_forbidden.cpp FALSE)
pae_expect_capability_compile(validated_copy validated_copy_forbidden.cpp FALSE)
pae_expect_capability_compile(budgeted_copy budgeted_copy_forbidden.cpp FALSE)
pae_expect_capability_compile(plan_draft_copy plan_draft_copy_forbidden.cpp FALSE)
pae_expect_capability_compile(raw_builder_entry raw_plan_builder_entry_forbidden.cpp FALSE)
pae_expect_capability_compile(schema_to_budgeted schema_to_budgeted_forbidden.cpp FALSE)

target_include_directories(
    pae_config_compiler_contract_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/config_compiler
)

target_link_libraries(
    pae_config_compiler_contract_tests
    PRIVATE
        pae_config_compiler
        pae::project_options
)

set(pae_config_compiler_test_data_dir ${CMAKE_CURRENT_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${pae_config_compiler_test_data_dir})
file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    DESTINATION ${pae_config_compiler_test_data_dir}
)
file(
    COPY ${CMAKE_CURRENT_SOURCE_DIR}/expected
    DESTINATION ${pae_config_compiler_test_data_dir}
)
configure_file(
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json
    ${pae_config_compiler_test_data_dir}/synthetic_lab_exchange_slice.pae.json
    COPYONLY
)

add_test(
    NAME pae.config_compiler.contract
    COMMAND pae_config_compiler_contract_tests data
)

set_tests_properties(
    pae.config_compiler.contract
    PROPERTIES
        LABELS "config_compiler;loader_schema_ir"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
