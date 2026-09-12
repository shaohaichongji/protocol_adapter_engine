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

if(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    add_executable(pae_ascii_stream_framing_contract_tests ascii_stream_framer_tests.cpp)
    target_link_libraries(
        pae_ascii_stream_framing_contract_tests
        PRIVATE
            pae_protocol_framing
            pae_config_compiler
            pae_protocol_core_slice_instrumented
            pae::project_options
    )
    add_test(
        NAME pae.protocol_framing.ascii_stream_contract
        COMMAND
            pae_ascii_stream_framing_contract_tests
            ${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json
    )
    set_tests_properties(
        pae.protocol_framing.ascii_stream_contract
        PROPERTIES LABELS "protocol_framing;offline;schema_v11;ascii_stream" TIMEOUT 60
    )
endif()
