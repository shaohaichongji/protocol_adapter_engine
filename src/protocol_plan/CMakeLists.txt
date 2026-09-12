add_library(
    pae_protocol_plan
    STATIC
        plan_bundle.cpp
        plan_bundle.h
        plan_builder.cpp
        plan_builder.h
        decimal_conversion_internal.cpp
        decimal_conversion_internal.h
        plan_draft_internal.h
        plan_memory.cpp
        plan_memory.h
        frozen_storage.h
        plan_types.h
)
add_library(pae::protocol_plan ALIAS pae_protocol_plan)

target_include_directories(
    pae_protocol_plan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    pae_protocol_plan
    PRIVATE
        pae::project_options
)

target_compile_features(pae_protocol_plan PUBLIC cxx_std_17)

if(PAE_ENABLE_SCHEMA_V05_COMPILER)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V05_COMPILER=1)
endif()
if(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V06_CRC_COMPILER=1)
endif()
if(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=1)
endif()
if(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=1)
endif()
if(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=1)
endif()
if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    target_compile_definitions(pae_protocol_plan PUBLIC PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=1)
endif()

set_target_properties(
    pae_protocol_plan
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
