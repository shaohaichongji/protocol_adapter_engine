add_library(
    pae_business_embedding_example_support
    STATIC
        business_adapter.cpp
        business_adapter.h
)
target_include_directories(
    pae_business_embedding_example_support
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
)
target_link_libraries(
    pae_business_embedding_example_support
    PUBLIC
        pae_protocol_core_slice
    PRIVATE
        pae_config_compiler
        pae::protocol_plan
        pae::project_options
)
set_target_properties(
    pae_business_embedding_example_support
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)

add_executable(pae_business_embedding_example main.cpp)
target_link_libraries(
    pae_business_embedding_example
    PRIVATE
        pae_business_embedding_example_support
        pae::project_options
)
