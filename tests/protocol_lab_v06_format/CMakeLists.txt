include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake")

pae_add_yyjson()

add_executable(
    pae_protocol_lab_v06_format_tests
    v06_format_tests.cpp
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/v06_format.cpp"
    "${PROJECT_SOURCE_DIR}/tools/protocol_lab/sha256.cpp"
)
target_include_directories(
    pae_protocol_lab_v06_format_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_v06_format_tests
    PRIVATE
        pae::project_options
        pae::yyjson
)
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    target_compile_definitions(
        pae_protocol_lab_v06_format_tests
        PRIVATE PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=1
    )
endif()
set_target_properties(
    pae_protocol_lab_v06_format_tests
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_test(
    NAME pae.tools.protocol_lab.v06_format
    COMMAND pae_protocol_lab_v06_format_tests
)
set_tests_properties(
    pae.tools.protocol_lab.v06_format
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;isolated"
        TIMEOUT 60
)
