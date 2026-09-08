include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake")

pae_add_yyjson()

set(
    pae_protocol_lab_v07_fixture
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json"
)

add_library(
    pae_protocol_lab_v07_evidence_reader STATIC
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v07_run_evidence.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_evidence.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_format.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/sha256.cpp"
)
target_include_directories(
    pae_protocol_lab_v07_evidence_reader
    PUBLIC "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_v07_evidence_reader
    PRIVATE pae::project_options pae::yyjson
)

add_executable(
    pae_protocol_lab_v07_reader_isolation_tests
    v07_reader_isolation_tests.cpp
)
target_link_libraries(
    pae_protocol_lab_v07_reader_isolation_tests
    PRIVATE pae_protocol_lab_v07_evidence_reader pae::project_options pae::yyjson
)

add_executable(
    pae_protocol_lab_v07_run_evidence_tests
    v07_run_evidence_tests.cpp
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v07_run.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_execution.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_values_compat_internal.cpp"
)
target_include_directories(
    pae_protocol_lab_v07_run_evidence_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_v07_run_evidence_tests
    PRIVATE
        pae_protocol_lab_v07_evidence_reader
        pae_config_compiler
        pae_protocol_core_slice_instrumented
        pae::protocol_plan
        pae::project_options
        pae::yyjson
)

add_test(
    NAME pae.tools.protocol_lab.v07_reader_isolation
    COMMAND pae_protocol_lab_v07_reader_isolation_tests
)
set_tests_properties(
    pae.tools.protocol_lab.v07_reader_isolation
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;evidence;reader;isolated"
        TIMEOUT 30
)

add_test(
    NAME pae.tools.protocol_lab.v07_run_evidence
    COMMAND pae_protocol_lab_v07_run_evidence_tests "${pae_protocol_lab_v07_fixture}"
)
set_tests_properties(
    pae.tools.protocol_lab.v07_run_evidence
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;execution;evidence;isolated"
        TIMEOUT 180
)
