add_executable(
    pae_business_embedding_tests
    business_embedding_tests.cpp
)
target_link_libraries(
    pae_business_embedding_tests
    PRIVATE
        pae_business_embedding_example_support
        pae::project_options
)
set(
    pae_business_embedding_test_configs
    "${PROJECT_SOURCE_DIR}/examples/business_embedding/config/synthetic_business_device_a.pae.json"
    "${PROJECT_SOURCE_DIR}/examples/business_embedding/config/synthetic_business_device_b.pae.json"
)
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    list(
        APPEND pae_business_embedding_test_configs
        "${PROJECT_SOURCE_DIR}/examples/business_embedding/config/synthetic_business_device_length.pae.json"
    )
endif()
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    list(
        APPEND pae_business_embedding_test_configs
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json"
    )
endif()
add_test(
    NAME pae.examples.business_embedding.contract
    COMMAND pae_business_embedding_tests ${pae_business_embedding_test_configs}
)
set(pae_business_embedding_test_labels "examples;business_embedding;offline;dec042b")
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    list(APPEND pae_business_embedding_test_labels bounded_variable)
endif()
set_tests_properties(
    pae.examples.business_embedding.contract
    PROPERTIES
        LABELS "${pae_business_embedding_test_labels}"
        TIMEOUT 180
)
