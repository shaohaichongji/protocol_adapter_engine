add_executable(
    pae_config_compiler_contract_tests
    config_compiler_contract_tests.cpp
    plan_builder_test_peer.cpp
    plan_builder_test_peer.h
)

add_executable(
    pae_config_compiler_ui_description_tests
    ui_description_tests.cpp
)

target_include_directories(
    pae_config_compiler_ui_description_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/config_compiler
)

target_link_libraries(
    pae_config_compiler_ui_description_tests
    PRIVATE
        pae_config_compiler
        pae::project_options
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
if(PAE_ENABLE_SCHEMA_V05_COMPILER)
    target_compile_definitions(
        pae_config_compiler_contract_tests
        PRIVATE PAE_ENABLE_SCHEMA_V05_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    target_compile_definitions(
        pae_config_compiler_contract_tests
        PRIVATE PAE_ENABLE_SCHEMA_V06_CRC_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    target_compile_definitions(
        pae_config_compiler_contract_tests
        PRIVATE PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    target_compile_definitions(
        pae_config_compiler_contract_tests
        PRIVATE PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=1
    )
endif()

if(PAE_ENABLE_SCHEMA_V05_COMPILER)
    target_compile_definitions(
        pae_config_compiler_ui_description_tests PRIVATE PAE_ENABLE_SCHEMA_V05_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    target_compile_definitions(
        pae_config_compiler_ui_description_tests PRIVATE PAE_ENABLE_SCHEMA_V06_CRC_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    target_compile_definitions(
        pae_config_compiler_ui_description_tests PRIVATE PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=1
    )
endif()
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    target_compile_definitions(
        pae_config_compiler_ui_description_tests PRIVATE PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=1
    )
endif()

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
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/valid/minimal_complete_record.pae.json
    ${pae_config_compiler_test_data_dir}/minimal_complete_record.pae.json
    COPYONLY
)
foreach(pae_ui_description_example
        synthetic_crc_slice.pae.json
        synthetic_length_slice.pae.json
        synthetic_bounded_variable_record.pae.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/examples/config/${pae_ui_description_example}
        ${pae_config_compiler_test_data_dir}/${pae_ui_description_example}
        COPYONLY
    )
endforeach()
if(PAE_ENABLE_SCHEMA_V05_COMPILER)
    configure_file(
        ${PROJECT_SOURCE_DIR}/examples/config/synthetic_decimal_compile_slice.pae.json
        ${pae_config_compiler_test_data_dir}/synthetic_decimal_compile_slice.pae.json
        COPYONLY
    )
endif()

add_test(
    NAME pae.config_compiler.contract
    COMMAND pae_config_compiler_contract_tests data
)

add_test(
    NAME pae.config_compiler.ui_description
    COMMAND pae_config_compiler_ui_description_tests data
)

if(WIN32 AND PAE_ENABLE_SCHEMA_V05_COMPILER)
    find_program(pae_config_compiler_powershell NAMES pwsh pwsh.exe REQUIRED)
    add_test(
        NAME pae.config_compiler.decimal_schema_contract
        COMMAND
            "${pae_config_compiler_powershell}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_decimal_compiler_schema_contract.ps1"
            -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
            -SamplePath "${PROJECT_SOURCE_DIR}/examples/config/synthetic_decimal_compile_slice.pae.json"
            -InheritedPath "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/valid/minimal_complete_record.pae.json"
    )
    set_tests_properties(
        pae.config_compiler.decimal_schema_contract
        PROPERTIES
            LABELS "config_compiler;schema;decimal_compiler"
            TIMEOUT 60
    )
endif()

set_tests_properties(
    pae.config_compiler.contract
    PROPERTIES
        LABELS "config_compiler;loader_schema_ir"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(
    pae.config_compiler.ui_description
    PROPERTIES
        LABELS "config_compiler;ui_description"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
