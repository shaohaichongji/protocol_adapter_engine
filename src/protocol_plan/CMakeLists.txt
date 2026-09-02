add_library(
    pae_protocol_plan
    STATIC
        plan_bundle.cpp
        plan_bundle.h
        plan_builder.cpp
        plan_builder.h
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

set_target_properties(
    pae_protocol_plan
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
