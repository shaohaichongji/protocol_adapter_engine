add_library(
    pae_protocol_lab_internal STATIC
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
        pae::yyjson_candidate
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
