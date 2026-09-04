add_executable(
    pae_protocol_conformance_runner
    main.cpp
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixture_reader.cpp"
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/fixture_reader.h"
)

target_include_directories(
    pae_protocol_conformance_runner
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
        "${PROJECT_SOURCE_DIR}/src/protocol_plan"
        "${PROJECT_SOURCE_DIR}/tests/protocol_core"
)

target_link_libraries(
    pae_protocol_conformance_runner
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae::protocol_plan
        pae::project_options
)

add_test(
    NAME pae.tools.protocol_conformance.synthetic_engine
    COMMAND
        pae_protocol_conformance_runner
        --config
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        --corpus-root
        "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden"
)

set_tests_properties(
    pae.tools.protocol_conformance.synthetic_engine
    PROPERTIES
        LABELS "tools;protocol_conformance;synthetic_engine"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
