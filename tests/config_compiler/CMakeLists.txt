add_executable(
    pae_config_compiler_contract_tests
    config_compiler_contract_tests.cpp
)

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
