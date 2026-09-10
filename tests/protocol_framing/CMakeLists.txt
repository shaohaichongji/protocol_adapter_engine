add_executable(pae_protocol_framing_contract_tests stream_framer_tests.cpp)
target_link_libraries(
    pae_protocol_framing_contract_tests
    PRIVATE
        pae_protocol_framing
        pae_config_compiler
        pae_protocol_core_slice_instrumented
        pae::project_options
)

add_test(
    NAME pae.protocol_framing.contract
    COMMAND
        pae_protocol_framing_contract_tests
        ${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/late_length_compaction.pae.json
)
set_tests_properties(
    pae.protocol_framing.contract
    PROPERTIES LABELS "protocol_framing;offline;schema_v09" TIMEOUT 30
)
