add_executable(pae_ascii_text_example main.cpp)
target_include_directories(
    pae_ascii_text_example
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
)
target_link_libraries(
    pae_ascii_text_example
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
)

if(PAE_BUILD_TESTING)
    add_test(
        NAME pae.example.ascii_text
        COMMAND pae_ascii_text_example
                "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
    )
    set_tests_properties(
        pae.example.ascii_text
        PROPERTIES LABELS "example;ascii_text" TIMEOUT 60
    )
endif()
