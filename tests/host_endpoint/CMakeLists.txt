add_executable(pae_host_endpoint_tests host_endpoint_tests.cpp)
target_link_libraries(pae_host_endpoint_tests PRIVATE pae_host_endpoint pae_config_compiler pae::project_options)
add_test(NAME pae.host_endpoint.contract COMMAND pae_host_endpoint_tests
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json
    ${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json)
set_tests_properties(pae.host_endpoint.contract PROPERTIES TIMEOUT 60 LABELS "host_endpoint;offline")
