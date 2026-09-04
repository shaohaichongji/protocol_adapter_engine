include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjsonCandidate.cmake")

pae_add_yyjson_candidate("${PROJECT_SOURCE_DIR}/cmake/yyjson_candidate.lock.json")

add_library(
    pae_config_compiler
    STATIC
        config_compiler.cpp
        config_compiler.h
        plan_snapshot.cpp
        schema_ir.h
        validation_pipeline_internal.h
)

target_include_directories(
    pae_config_compiler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    pae_config_compiler
    PRIVATE
        pae::project_options
        pae::protocol_plan
        pae::yyjson_candidate
)

set_target_properties(
    pae_config_compiler
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
