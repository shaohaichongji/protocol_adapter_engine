add_executable(pae_ascii_stream_framing_example main.cpp)
target_link_libraries(
    pae_ascii_stream_framing_example
    PRIVATE
        pae_protocol_framing
        pae_protocol_core_slice
        pae_config_compiler
        pae::project_options
)

if(PAE_BUILD_TESTING)
    add_test(
        NAME pae.examples.ascii_stream_framing.host_push
        COMMAND
            pae_ascii_stream_framing_example
            ${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json
    )
    set_tests_properties(
        pae.examples.ascii_stream_framing.host_push
        PROPERTIES LABELS "example;offline;protocol_framing;schema_v11;ascii_stream" TIMEOUT 30
    )
endif()
