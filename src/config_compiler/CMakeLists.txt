include("${PROJECT_SOURCE_DIR}/cmake/PaeYyjson.cmake")

pae_add_yyjson()

add_library(
    pae_config_compiler
    STATIC
        config_compiler.cpp
        config_compiler.h
        plan_snapshot.cpp
        schema_ir.h
        ui_description.cpp
        ui_description.h
        ui_description_internal.h
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
        pae::yyjson
)

set_target_properties(
    pae_config_compiler
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
