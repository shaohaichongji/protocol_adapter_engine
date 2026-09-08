include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake")

pae_add_yyjson()

set(
    pae_protocol_lab_v06_execution_fixture
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json"
)

add_executable(
    pae_protocol_lab_v06_execution_tests
    v06_execution_tests.cpp
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_execution.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_values_compat_internal.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_format.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/sha256.cpp"
)
target_include_directories(
    pae_protocol_lab_v06_execution_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_v06_execution_tests
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice_instrumented
        pae::protocol_plan
        pae::project_options
        pae::yyjson
)
set_target_properties(
    pae_protocol_lab_v06_execution_tests
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_test(
    NAME pae.tools.protocol_lab.v06_execution
    COMMAND pae_protocol_lab_v06_execution_tests "${pae_protocol_lab_v06_execution_fixture}"
)
set_tests_properties(
    pae.tools.protocol_lab.v06_execution
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;execution;isolated"
        TIMEOUT 180
)
