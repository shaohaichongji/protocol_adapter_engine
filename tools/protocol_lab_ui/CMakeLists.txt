if(NOT PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    message(
        FATAL_ERROR
        "PAE_BUILD_PROTOCOL_LAB_UI requires PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON for the Schema 0.5-0.8 UI contract."
    )
endif()

set(
    pae_protocol_lab_ui_max_fixture
    "${PROJECT_BINARY_DIR}/generated/protocol_lab_ui/synthetic_ui_max.pae.json"
)
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_OUTPUT_FILE=${pae_protocol_lab_ui_max_fixture}"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateProtocolLabUiMaxFixture.cmake"
    RESULT_VARIABLE pae_protocol_lab_ui_max_fixture_result
)
if(NOT pae_protocol_lab_ui_max_fixture_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate the public maximum Protocol Lab UI fixture.")
endif()

add_library(
    pae_protocol_lab_ui_headless_internal
    STATIC
        compile_worker.cpp
        compile_worker.h
        ascii_escaped_input.cpp
        ascii_escaped_input.h
        description_mapping.cpp
        description_mapping.h
        document_session.cpp
        document_session.h
        inspect_hex_input.cpp
        inspect_hex_input.h
)
target_include_directories(
    pae_protocol_lab_ui_headless_internal
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    pae_protocol_lab_ui_headless_internal
    PUBLIC
        pae_protocol_lab_c1_execution_internal
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
)
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    if(NOT TARGET pae_protocol_lab_ascii_adapter_internal)
        message(FATAL_ERROR "Schema 0.10 Protocol Lab UI requires the ASCII adapter target.")
    endif()
    target_link_libraries(
        pae_protocol_lab_ui_headless_internal
        PUBLIC pae_protocol_lab_ascii_adapter_internal
    )
endif()
set_target_properties(
    pae_protocol_lab_ui_headless_internal
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_executable(
    pae_protocol_lab_ui
        application_window.cpp
        application_window.h
        document_tab.cpp
        document_tab.h
        exact_value_delegate.cpp
        exact_value_delegate.h
        field_table_model.cpp
        field_table_model.h
        hex_view.cpp
        hex_view.h
        main.cpp
)
target_link_libraries(
    pae_protocol_lab_ui
    PRIVATE
        pae_protocol_lab_ui_headless_internal
        PAE::Qt5Widgets
        pae::project_options
)
set_target_properties(
    pae_protocol_lab_ui
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

set(
    pae_protocol_lab_ui_deployment_configs
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ui_v05.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ui_v06.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ui_v07.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ui_v08.pae.json"
        "${pae_protocol_lab_ui_max_fixture}"
)
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    list(
        APPEND pae_protocol_lab_ui_deployment_configs
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_literal_only.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ascii_decode_only.pae.json"
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_ascii_encode_only.pae.json"
    )
endif()
pae_configure_protocol_lab_ui_deployment(
    pae_protocol_lab_ui
    CONFIG_FILES ${pae_protocol_lab_ui_deployment_configs}
)
