set(pae_lab_binary_test_source_root "${PROJECT_SOURCE_DIR}")
if(DEFINED PAE_LAB_STANDALONE_TEST_SOURCE_ROOT)
    set(pae_lab_binary_test_source_root "${PAE_LAB_STANDALONE_TEST_SOURCE_ROOT}")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1)
    add_executable(pae_binary_public_h1_tests public_binary_decode_tests.cpp)
    target_link_libraries(pae_binary_public_h1_tests PRIVATE pae_protocol_lab_binary_public_h1 pae::project_options)
    add_test(NAME pae.protocol_lab_binary.public_h1 COMMAND pae_binary_public_h1_tests ${pae_lab_binary_test_source_root})
    set_tests_properties(pae.protocol_lab_binary.public_h1 PROPERTIES TIMEOUT 60 LABELS "lab_binary;public_h1;offline")
endif()

if(PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER)
add_executable(pae_binary_candidate_materializer_tests candidate_materializer_tests.cpp)
target_link_libraries(pae_binary_candidate_materializer_tests PRIVATE pae_protocol_lab_binary_materializer pae_config_compiler pae::project_options)
add_test(NAME pae.protocol_lab_binary.materializer COMMAND pae_binary_candidate_materializer_tests
    ${PROJECT_SOURCE_DIR}/tests/protocol_core/fixtures/decimal_core_contract.pae.json
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_int64_slice.pae.json
    ${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json)
set_tests_properties(pae.protocol_lab_binary.materializer PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_owned_description_tests owned_description_tests.cpp)
target_link_libraries(pae_binary_owned_description_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.description COMMAND pae_binary_owned_description_tests
    ${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures)
set_tests_properties(pae.protocol_lab_binary.description PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_prepared_tests prepared_binary_tests.cpp)
target_link_libraries(pae_binary_prepared_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.prepared COMMAND pae_binary_prepared_tests ${PROJECT_SOURCE_DIR})
set_tests_properties(pae.protocol_lab_binary.prepared PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_stream_tests stream_tests.cpp)
target_link_libraries(pae_binary_stream_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.stream COMMAND pae_binary_stream_tests ${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json)
set_tests_properties(pae.protocol_lab_binary.stream PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_saved_state_tests saved_state_tests.cpp)
target_link_libraries(pae_binary_saved_state_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.saved_state COMMAND pae_binary_saved_state_tests ${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json)
set_tests_properties(pae.protocol_lab_binary.saved_state PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_encode_tests encode_tests.cpp)
target_link_libraries(pae_binary_encode_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.encode COMMAND pae_binary_encode_tests ${PROJECT_SOURCE_DIR})
set_tests_properties(pae.protocol_lab_binary.encode PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")

add_executable(pae_binary_integration_audit_tests integration_audit_tests.cpp)
target_link_libraries(pae_binary_integration_audit_tests PRIVATE pae_protocol_lab_binary_materializer pae::project_options)
add_test(NAME pae.protocol_lab_binary.integration_audit COMMAND pae_binary_integration_audit_tests ${PROJECT_SOURCE_DIR})
set_tests_properties(pae.protocol_lab_binary.integration_audit PROPERTIES TIMEOUT 60 LABELS "lab_binary;offline")
endif()
