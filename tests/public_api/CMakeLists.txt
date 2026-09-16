add_executable(pae_public_api_tests public_api_tests.cpp)
target_link_libraries(pae_public_api_tests PRIVATE PAE::pae)
target_include_directories(pae_public_api_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/public_api")

add_executable(pae_public_codec_tests public_codec_tests.cpp)
target_link_libraries(pae_public_codec_tests PRIVATE PAE::pae)
target_include_directories(pae_public_codec_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/public_api")

add_executable(pae_public_consumer_metadata_tests public_consumer_metadata_tests.cpp)
target_link_libraries(pae_public_consumer_metadata_tests PRIVATE PAE::pae)

add_executable(pae_public_physical_query_tests public_physical_query_tests.cpp)
target_link_libraries(pae_public_physical_query_tests PRIVATE PAE::pae)
target_include_directories(pae_public_physical_query_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/public_api")

add_executable(pae_public_ascii_facts_tests public_ascii_facts_tests.cpp)
target_link_libraries(pae_public_ascii_facts_tests PRIVATE PAE::pae)

add_executable(pae_public_stream_framer_tests public_stream_framer_tests.cpp)
target_link_libraries(pae_public_stream_framer_tests PRIVATE PAE::pae)

add_executable(pae_public_host_endpoint_tests public_host_endpoint_tests.cpp)
target_link_libraries(pae_public_host_endpoint_tests PRIVATE PAE::pae)
target_include_directories(pae_public_host_endpoint_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/public_api")

add_executable(
    pae_public_header_compile_tests
    header_main.cpp
    header_export.cpp
    header_version.cpp
    header_description.cpp
    header_compiler.cpp
    header_codec.cpp
    header_host_endpoint.cpp
    header_stream_framer.cpp
)
target_link_libraries(pae_public_header_compile_tests PRIVATE PAE::pae)

add_test(
    NAME pae.public_api.compile_metadata
    COMMAND
        pae_public_api_tests
        "${PROJECT_SOURCE_DIR}/tests/config_compiler/fixtures/valid/minimal_complete_record.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
)
add_test(NAME pae.public_api.header_self_contained COMMAND pae_public_header_compile_tests)
add_test(
    NAME pae.public_api.complete_record_codec
    COMMAND
        pae_public_codec_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_int64_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
)
add_test(
    NAME pae.public_api.consumer_metadata
    COMMAND
        pae_public_consumer_metadata_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
)
add_test(
    NAME pae.public_api.physical_query
    COMMAND
        pae_public_physical_query_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bitfield_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_int64_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json"
)
add_test(
    NAME pae.public_api.ascii_facts
    COMMAND pae_public_ascii_facts_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab/fixtures/ambiguous.pae.json"
)
add_test(
    NAME pae.public_api.stream_framer
    COMMAND
        pae_public_stream_framer_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
)
add_test(
    NAME pae.public_api.host_endpoint
    COMMAND
        pae_public_host_endpoint_tests
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
)
add_test(
    NAME pae.public_api.header_boundary
    COMMAND
        "${CMAKE_COMMAND}"
        -DPUBLIC_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_public_headers.cmake"
)

set_tests_properties(
    pae.public_api.compile_metadata
    pae.public_api.header_self_contained
    pae.public_api.header_boundary
    PROPERTIES LABELS "public_api;compiler;metadata" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.complete_record_codec
    PROPERTIES LABELS "public_api;codec;complete_record" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.consumer_metadata
    PROPERTIES LABELS "public_api;compiler;metadata;codec;complete_record" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.physical_query
    PROPERTIES LABELS "public_api;compiler;metadata;host;codec;complete_record" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.ascii_facts
    PROPERTIES LABELS "public_api;compiler;metadata;host;codec;complete_record" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.stream_framer
    PROPERTIES LABELS "public_api;stream_framer;codec;offline" TIMEOUT 180
)
set_tests_properties(
    pae.public_api.host_endpoint
    PROPERTIES LABELS "public_api;host;stream_framer;codec;offline" TIMEOUT 180
)
