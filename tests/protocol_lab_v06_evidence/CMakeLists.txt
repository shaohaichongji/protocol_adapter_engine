include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake")

pae_add_yyjson()

add_executable(
    pae_protocol_lab_v06_evidence_tests
    v06_evidence_tests.cpp
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_evidence.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_format.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/sha256.cpp"
)
target_include_directories(
    pae_protocol_lab_v06_evidence_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_v06_evidence_tests
    PRIVATE
        pae::project_options
        pae::yyjson
)
set_target_properties(
    pae_protocol_lab_v06_evidence_tests
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_test(
    NAME pae.tools.protocol_lab.v06_evidence
    COMMAND pae_protocol_lab_v06_evidence_tests
)
set_tests_properties(
    pae.tools.protocol_lab.v06_evidence
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;evidence;isolated"
        TIMEOUT 60
)
