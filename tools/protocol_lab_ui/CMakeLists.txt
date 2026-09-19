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
    pae_protocol_lab_ui_owned_presentation
    STATIC
        canonical_input.cpp
        canonical_input.h
        ascii_host_types.cpp
        ascii_host_types.h
        owned_presentation_types.h
        ui_physical_types.h
)

if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    add_library(
        pae_protocol_lab_ui_ascii_types_compat
        STATIC
            ascii_host_types_compat.cpp
            ascii_host_types_compat.h
    )
    target_include_directories(
        pae_protocol_lab_ui_ascii_types_compat
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(
        pae_protocol_lab_ui_ascii_types_compat
        PUBLIC pae_protocol_lab_ui_owned_presentation PAE::pae
        PRIVATE pae_protocol_lab_ascii_adapter_internal pae::project_options
    )
    set_target_properties(
        pae_protocol_lab_ui_ascii_types_compat
        PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    )
endif()
target_include_directories(
    pae_protocol_lab_ui_owned_presentation
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    pae_protocol_lab_ui_owned_presentation
    PUBLIC PAE::pae
    PRIVATE pae::project_options
)
set_target_properties(
    pae_protocol_lab_ui_owned_presentation
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    if(NOT TARGET pae_protocol_lab_ascii_public_a1)
        message(FATAL_ERROR "ASCII public UI facade requires the public A1 adapter target.")
    endif()
    add_library(
        pae_protocol_lab_ui_ascii_public_facade
        STATIC
            ascii_host_adapter.cpp
            ascii_host_adapter.h
            ascii_host_types.h
    )
    target_include_directories(
        pae_protocol_lab_ui_ascii_public_facade
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(
        pae_protocol_lab_ui_ascii_public_facade
        PUBLIC
            pae_protocol_lab_ui_owned_presentation
            pae_protocol_lab_ascii_public_a1
            PAE::pae
        PRIVATE pae::project_options
    )
    set_target_properties(
        pae_protocol_lab_ui_ascii_public_facade
        PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    )
    if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
        add_library(
            pae_protocol_lab_ui_ascii_private_compat
            STATIC
                ascii_host_adapter_compat.cpp
                ascii_host_adapter_compat.h
        )
        target_include_directories(
            pae_protocol_lab_ui_ascii_private_compat
            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_link_libraries(
            pae_protocol_lab_ui_ascii_private_compat
            PUBLIC pae_protocol_lab_ui_ascii_public_facade
            PRIVATE
                pae_protocol_lab_ui_ascii_types_compat
                pae_protocol_lab_ascii_adapter_internal
                pae::project_options
        )
        set_target_properties(
            pae_protocol_lab_ui_ascii_private_compat
            PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
        )
    endif()
endif()

add_library(
    pae_protocol_lab_ui_headless_internal
    STATIC
        compile_worker.cpp
        compile_worker.h
        schema_dispatch.cpp
        schema_dispatch.h
        binary_host_adapter.h
        ascii_escaped_input.cpp
        ascii_escaped_input.h
        description_mapping.cpp
        description_mapping.h
        document_session.cpp
        document_session.h
        inspect_hex_input.cpp
        inspect_hex_input.h
)
if(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
    target_sources(
        pae_protocol_lab_ui_headless_internal
        PRIVATE
            public_legacy_complete_adapter.cpp
            public_legacy_complete_adapter.h
    )
    target_link_libraries(
        pae_protocol_lab_ui_headless_internal
        PUBLIC PAE::pae
    )
    target_compile_definitions(
        pae_protocol_lab_ui_headless_internal
        PUBLIC PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE=1
    )
    if(PAE_BUILD_TESTING)
        # Public because the adapter class layout is visible to linked test consumers.
        target_compile_definitions(
            pae_protocol_lab_ui_headless_internal
            PUBLIC PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION=1
        )
    endif()
endif()
target_include_directories(
    pae_protocol_lab_ui_headless_internal
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    pae_protocol_lab_ui_headless_internal
    PUBLIC
        pae_protocol_lab_ui_owned_presentation
        pae_protocol_lab_c1_execution_internal
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
        pae::yyjson
)
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    if(NOT TARGET pae_protocol_lab_ascii_adapter_internal)
        message(FATAL_ERROR "Schema 0.10 Protocol Lab UI requires the ASCII adapter target.")
    endif()
    target_link_libraries(
        pae_protocol_lab_ui_headless_internal
        PUBLIC pae_protocol_lab_ascii_adapter_internal
        PRIVATE pae_protocol_lab_ui_ascii_types_compat
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    if(NOT PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        if(NOT TARGET pae_protocol_lab_binary_materializer)
            message(FATAL_ERROR "Schema 0.9 Binary Protocol Lab UI requires the Binary materializer target.")
        endif()
        target_sources(pae_protocol_lab_ui_headless_internal PRIVATE binary_host_adapter.cpp)
        target_link_libraries(
            pae_protocol_lab_ui_headless_internal
            PUBLIC pae_protocol_lab_binary_materializer
        )
    endif()
    target_compile_definitions(
        pae_protocol_lab_ui_headless_internal
        PUBLIC PAE_BUILD_PROTOCOL_LAB_BINARY_UI=1
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if(NOT TARGET pae_protocol_lab_binary_public_h1)
        message(FATAL_ERROR "Binary public H2 requires the public H1 adapter target.")
    endif()
    add_library(pae_protocol_lab_ui_binary_public_h2 STATIC
        binary_host_adapter_public.cpp binary_host_adapter_public.h
        public_binary_description.cpp public_binary_description.h)
    target_include_directories(pae_protocol_lab_ui_binary_public_h2 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(pae_protocol_lab_ui_binary_public_h2 PUBLIC pae_protocol_lab_binary_public_h1 PRIVATE pae::project_options)
    foreach(pae_ui_dto_option IN ITEMS PAE_ENABLE_SCHEMA_V05_COMPILER PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
        if(${pae_ui_dto_option})
            target_compile_definitions(pae_protocol_lab_ui_binary_public_h2 PUBLIC "${pae_ui_dto_option}=1")
        endif()
    endforeach()
    set_target_properties(pae_protocol_lab_ui_binary_public_h2 PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    target_link_libraries(pae_protocol_lab_ui_headless_internal PUBLIC pae_protocol_lab_ui_binary_public_h2)
    target_compile_definitions(pae_protocol_lab_ui_headless_internal PUBLIC PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2=1)
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    target_link_libraries(
        pae_protocol_lab_ui_headless_internal
        PUBLIC pae_protocol_lab_ui_ascii_public_facade
    )
    if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
        target_link_libraries(
            pae_protocol_lab_ui_headless_internal
            PRIVATE pae_protocol_lab_ui_ascii_private_compat
        )
    endif()
    target_compile_definitions(
        pae_protocol_lab_ui_headless_internal
        PUBLIC PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=1
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
    target_compile_definitions(
        pae_protocol_lab_ui_headless_internal
        PUBLIC PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI=1
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
if(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    target_compile_definitions(pae_protocol_lab_ui PRIVATE PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC=1)
    target_sources(pae_protocol_lab_ui PRIVATE ascii_smoke_diagnostic.h)
endif()
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
if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    list(
        APPEND pae_protocol_lab_ui_deployment_configs
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    list(APPEND pae_protocol_lab_ui_deployment_configs
        "${PROJECT_SOURCE_DIR}/tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json"
    )
endif()
pae_configure_protocol_lab_ui_deployment(
    pae_protocol_lab_ui
    CONFIG_FILES ${pae_protocol_lab_ui_deployment_configs}
)
