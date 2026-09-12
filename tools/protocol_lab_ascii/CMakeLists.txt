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
set_target_properties(
    pae_protocol_lab_ascii_adapter_internal
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
