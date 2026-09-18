if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
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
if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    add_executable(pae_lab_host_adapter_tests host_observer_adapter_tests.cpp)
    target_link_libraries(pae_lab_host_adapter_tests PRIVATE pae_protocol_lab_ascii_adapter_internal pae::project_options)
    add_test(NAME pae.tools.lab_host.adapter COMMAND pae_lab_host_adapter_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json")
    set_tests_properties(pae.tools.lab_host.adapter PROPERTIES TIMEOUT 60 LABELS "host_endpoint;headless;offline")
endif()
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1)
    add_executable(pae_protocol_lab_ascii_public_a1_tests public_ascii_offline_adapter_tests.cpp)
    target_link_libraries(
        pae_protocol_lab_ascii_public_a1_tests
        PRIVATE pae_protocol_lab_ascii_public_a1 pae::project_options
    )
    add_test(
        NAME pae.tools.protocol_lab_ascii.public_a1
        COMMAND pae_protocol_lab_ascii_public_a1_tests "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ascii.public_a1
        PROPERTIES LABELS "tools;protocol_lab_ascii;public_a1;offline" TIMEOUT 60
    )
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM)
    add_executable(
        pae_protocol_lab_ascii_public_stream_tests
        public_ascii_stream_adapter_tests.cpp
    )
    target_link_libraries(
        pae_protocol_lab_ascii_public_stream_tests
        PRIVATE pae_protocol_lab_ascii_public_a1 pae::project_options
    )
    add_test(
        NAME pae.tools.protocol_lab_ascii.public_stream
        COMMAND pae_protocol_lab_ascii_public_stream_tests "${PROJECT_SOURCE_DIR}"
    )
    add_test(
        NAME pae.tools.protocol_lab_ascii.public_stream_noexcept_reject
        COMMAND pae_protocol_lab_ascii_public_stream_tests
                --diagnostic-allocation-probe reject "${PROJECT_SOURCE_DIR}"
    )
    add_test(
        NAME pae.tools.protocol_lab_ascii.public_stream_noexcept_candidate
        COMMAND pae_protocol_lab_ascii_public_stream_tests
                --diagnostic-allocation-probe candidate "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ascii.public_stream
        pae.tools.protocol_lab_ascii.public_stream_noexcept_reject
        pae.tools.protocol_lab_ascii.public_stream_noexcept_candidate
        PROPERTIES LABELS "tools;protocol_lab_ascii;public;stream;headless" TIMEOUT 60
    )
endif()
