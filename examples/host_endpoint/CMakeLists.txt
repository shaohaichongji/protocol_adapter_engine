add_executable(pae_host_endpoint_example main.cpp)
target_link_libraries(pae_host_endpoint_example PRIVATE pae_host_endpoint pae_config_compiler pae::project_options)
if(PAE_BUILD_TESTING)
    add_test(NAME pae.examples.host_endpoint COMMAND pae_host_endpoint_example
        ${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json)
    set_tests_properties(pae.examples.host_endpoint PROPERTIES TIMEOUT 30 LABELS "host_endpoint;offline")
endif()
