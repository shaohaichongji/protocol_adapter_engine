if((PAE_BUILD_PROTOCOL_LAB AND PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05) OR
   PAE_BUILD_PROTOCOL_LAB_UI)
    add_library(
        pae_protocol_lab_c1_execution_internal STATIC
        exact_value_text_internal.cpp
        exact_value_text_internal.h
        sha256.cpp
        sha256.h
        v06_execution.cpp
        v06_execution.h
        v06_format.cpp
        v06_format.h
        v06_values_compat_internal.cpp
        v06_values_compat_internal.h
    )
    target_include_directories(
        pae_protocol_lab_c1_execution_internal
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/config_compiler"
            "${PROJECT_SOURCE_DIR}/src/protocol_core"
    )
    target_link_libraries(
        pae_protocol_lab_c1_execution_internal
        PUBLIC
            pae::protocol_plan
        PRIVATE
            pae_config_compiler
            pae_protocol_core_slice
            pae::project_options
            pae::yyjson
    )
    set_target_properties(
        pae_protocol_lab_c1_execution_internal
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
endif()

if(PAE_BUILD_PROTOCOL_LAB)
    add_library(
        pae_protocol_lab_internal STATIC
        cli_path_encoding_internal.cpp
        cli_path_encoding_internal.h
        cli_options.cpp
        cli_options.h
        evidence_bundle.cpp
        evidence_bundle.h
        lab_types.h
        protocol_operations.cpp
        protocol_operations.h
        protocol_lab.cpp
        protocol_lab.h
        result_format.cpp
        result_format.h
        sha256.cpp
        sha256.h
        udp_exchange.cpp
        udp_exchange.h
    )

    if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05)
        target_sources(
            pae_protocol_lab_internal
            PRIVATE
                c3_cli.cpp
                c3_cli.h
                v06_evidence.cpp
                v06_evidence.h
                v07_run.cpp
                v07_run.h
                v07_run_evidence.cpp
                v07_run_evidence.h
        )
        target_link_libraries(
            pae_protocol_lab_internal
            PRIVATE pae_protocol_lab_c1_execution_internal
        )
        target_compile_definitions(
            pae_protocol_lab_internal
            PRIVATE PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05=1
        )
        if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
            target_compile_definitions(
                pae_protocol_lab_internal
                PRIVATE PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC=1
            )
        endif()
        if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
            target_compile_definitions(
                pae_protocol_lab_internal
                PRIVATE PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH=1
            )
        endif()
        if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE)
            target_compile_definitions(
                pae_protocol_lab_internal
                PRIVATE PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE=1
            )
        endif()
        if(PAE_BUILD_TESTING)
            target_compile_definitions(
                pae_protocol_lab_internal
                PRIVATE PAE_ENABLE_C3_BINDING_TESTS=1
            )
        endif()
    endif()

    target_include_directories(
        pae_protocol_lab_internal
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${PROJECT_SOURCE_DIR}/src/config_compiler"
            "${PROJECT_SOURCE_DIR}/src/protocol_core"
            "${PROJECT_SOURCE_DIR}/src/protocol_plan"
    )

    target_link_libraries(
        pae_protocol_lab_internal
        PRIVATE
            pae_config_compiler
            pae_protocol_core_slice
            pae::protocol_plan
            pae::project_options
            pae::yyjson
    )

    if(WIN32)
        target_link_libraries(pae_protocol_lab_internal PRIVATE ws2_32)
    endif()

    set_target_properties(
        pae_protocol_lab_internal
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )

    add_executable(pae_protocol_lab main.cpp)
    target_link_libraries(pae_protocol_lab PRIVATE pae_protocol_lab_internal pae::project_options)
    set_target_properties(
        pae_protocol_lab
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
endif()
