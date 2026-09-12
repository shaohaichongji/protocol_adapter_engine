add_executable(
    pae_protocol_lab_ascii_adapter_tests
    ascii_offline_adapter_tests.cpp
)
target_link_libraries(
    pae_protocol_lab_ascii_adapter_tests
    PRIVATE
        pae_protocol_lab_ascii_adapter_internal
        pae::project_options
)
add_test(
    NAME pae.tools.protocol_lab_ascii.adapter
    COMMAND
        pae_protocol_lab_ascii_adapter_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_literal_only.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/config_compiler/fixtures/valid/minimal_complete_record.pae.json"
        $<$<BOOL:${PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER}>:${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json>
)
set_tests_properties(
    pae.tools.protocol_lab_ascii.adapter
    PROPERTIES
        LABELS "tools;protocol_lab_ascii;offline;headless"
        TIMEOUT 180
)
