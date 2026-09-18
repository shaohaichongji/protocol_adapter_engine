if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
add_library(
    pae_protocol_lab_ascii_adapter_internal
    STATIC
        ascii_offline_adapter.cpp
        ascii_offline_adapter.h
)
target_include_directories(
    pae_protocol_lab_ascii_adapter_internal
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
target_link_libraries(
    pae_protocol_lab_ascii_adapter_internal
    PUBLIC
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
    PRIVATE
        pae::project_options
)
if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    target_link_libraries(pae_protocol_lab_ascii_adapter_internal PUBLIC pae::protocol_framing)
    target_compile_definitions(
        pae_protocol_lab_ascii_adapter_internal
        PUBLIC PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=1
    )
endif()
if(PAE_BUILD_TESTING)
    target_compile_definitions(
        pae_protocol_lab_ascii_adapter_internal
        PUBLIC PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS=1
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    target_sources(pae_protocol_lab_ascii_adapter_internal PRIVATE host_observer_adapter.cpp host_observer_adapter.h)
    target_link_libraries(pae_protocol_lab_ascii_adapter_internal PUBLIC pae_host_endpoint)
    target_compile_definitions(pae_protocol_lab_ascii_adapter_internal PUBLIC PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER=1)
endif()
set_target_properties(
    pae_protocol_lab_ascii_adapter_internal
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1)
    add_library(
        pae_protocol_lab_ascii_public_a1
        STATIC
            public_ascii_offline_adapter.cpp
            public_ascii_offline_adapter.h
    )
    if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
        target_sources(
            pae_protocol_lab_ascii_public_a1
            PRIVATE
                public_ascii_host_adapter.cpp
                public_ascii_host_adapter.h
        )
        target_compile_definitions(
            pae_protocol_lab_ascii_public_a1
            PUBLIC PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=1
        )
    endif()
    target_include_directories(
        pae_protocol_lab_ascii_public_a1
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    )
    target_link_libraries(
        pae_protocol_lab_ascii_public_a1
        PUBLIC
            PAE::pae
        PRIVATE
            pae::project_options
    )
    if(PAE_BUILD_TESTING)
        target_compile_definitions(
            pae_protocol_lab_ascii_public_a1
            PUBLIC PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS=1
        )
    endif()
    set_target_properties(
        pae_protocol_lab_ascii_public_a1
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
endif()
