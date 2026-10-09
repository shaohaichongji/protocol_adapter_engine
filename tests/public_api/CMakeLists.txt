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

add_executable(pae_public_source_charset_tests source_charset_consumer.cpp)
target_link_libraries(pae_public_source_charset_tests PRIVATE PAE::pae)
get_target_property(pae_public_compile_options PAE::pae INTERFACE_COMPILE_OPTIONS)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/public-compile-options.txt" "${pae_public_compile_options}")
unset(pae_public_compile_options)
add_test(NAME pae.public_api.source_charset COMMAND pae_public_source_charset_tests)
if(MSVC)
    add_executable(pae_public_charset_host_tests source_charset_consumer.cpp)
    target_link_libraries(pae_public_charset_host_tests PRIVATE PAE::pae)
    set_property(TARGET pae_public_charset_host_tests PROPERTY PAE_MSVC_SOURCE_CHARSET_SUPPLIED ON)
    target_compile_options(pae_public_charset_host_tests PRIVATE /utf-8)
    target_compile_definitions(pae_public_charset_host_tests PRIVATE PAE_TEST_EXPECT_UTF8_EXECUTION=1)

    add_executable(pae_public_charset_project_tests source_charset_consumer.cpp)
    target_link_libraries(pae_public_charset_project_tests PRIVATE pae::project_options PAE::pae)
    target_compile_definitions(pae_public_charset_project_tests PRIVATE PAE_TEST_EXPECT_UTF8_EXECUTION=1)

    add_executable(pae_public_charset_execution936_tests source_charset_consumer.cpp)
    target_link_libraries(pae_public_charset_execution936_tests PRIVATE PAE::pae)
    target_compile_options(pae_public_charset_execution936_tests PRIVATE /execution-charset:.936)
    target_compile_definitions(pae_public_charset_execution936_tests PRIVATE PAE_TEST_EXPECT_936_EXECUTION=1)

    foreach(pair IN ITEMS "default|pae_public_source_charset_tests" "host|pae_public_charset_host_tests"
                          "project|pae_public_charset_project_tests" "execution936|pae_public_charset_execution936_tests")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 name)
        list(GET pair 1 target)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/charset/$<CONFIG>/$<COMPILE_LANGUAGE>/${name}.txt"
            CONTENT "$<TARGET_GENEX_EVAL:${target},$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>>")
    endforeach()
    foreach(kind IN ITEMS host project execution936)
        add_test(NAME pae.public_api.source_charset_${kind} COMMAND pae_public_charset_${kind}_tests)
        set_tests_properties(pae.public_api.source_charset_${kind}
            PROPERTIES LABELS "public_api;encoding;offline" TIMEOUT 30)
    endforeach()
endif()
add_test(
    NAME pae.public_api.source_charset_options
    COMMAND "${CMAKE_COMMAND}"
        "-DOPTIONS_FILE=${CMAKE_CURRENT_BINARY_DIR}/public-compile-options.txt"
        "-DSDK_CONFIG_ROOT=${PROJECT_BINARY_DIR}/sdk"
        "-DEVALUATED_ROOT=$<$<BOOL:${MSVC}>:${CMAKE_CURRENT_BINARY_DIR}/charset>"
        "-DCONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_source_charset.cmake"
)
set_tests_properties(
    pae.public_api.source_charset pae.public_api.source_charset_options
    PROPERTIES LABELS "public_api;encoding;offline" TIMEOUT 30
)

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
