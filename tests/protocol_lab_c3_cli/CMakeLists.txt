set(pae_protocol_lab_c3_fixture_dir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(pae_protocol_lab_c3_policy_tests c3_policy_tests.cpp)
target_compile_definitions(pae_protocol_lab_c3_policy_tests PRIVATE PAE_ENABLE_C3_BINDING_TESTS=1)
target_include_directories(
    pae_protocol_lab_c3_policy_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_c3_policy_tests
    PRIVATE pae_protocol_lab_internal pae::project_options pae::yyjson
)

add_test(
    NAME pae.tools.protocol_lab.c3_cli
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DPAE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DPAE_FIXTURE_DIR=${pae_protocol_lab_c3_fixture_dir}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_c3_cli.cmake"
)
set_tests_properties(
    pae.tools.protocol_lab.c3_cli
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;c3;cli"
        TIMEOUT 180
)

if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
    set(
        pae_protocol_lab_crc_cli_arguments
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DPAE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DPAE_FIXTURE_DIR=${pae_protocol_lab_c3_fixture_dir}"
    )
    if(TARGET pae_protocol_lab_v07_run_evidence_tests)
        list(
            APPEND
            pae_protocol_lab_crc_cli_arguments
            "-DPAE_HISTORY_GENERATION_FIXTURE=$<TARGET_FILE:pae_protocol_lab_v07_run_evidence_tests>"
        )
    endif()
    add_test(
        NAME pae.tools.protocol_lab.crc_cli
        COMMAND
            "${CMAKE_COMMAND}"
            ${pae_protocol_lab_crc_cli_arguments}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_crc_cli.cmake"
    )
    set_tests_properties(
        pae.tools.protocol_lab.crc_cli
        PROPERTIES LABELS "tools;protocol_lab;offline;crc;cli" TIMEOUT 180
    )
endif()

if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
    add_test(
        NAME pae.tools.protocol_lab.length_cli
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
            "-DPAE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DPAE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DPAE_FIXTURE_DIR=${pae_protocol_lab_c3_fixture_dir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_length_cli.cmake"
    )
    set_tests_properties(
        pae.tools.protocol_lab.length_cli
        PROPERTIES LABELS "tools;protocol_lab;offline;length_field;cli" TIMEOUT 180
    )
endif()

add_test(NAME pae.tools.protocol_lab.c3_policy COMMAND pae_protocol_lab_c3_policy_tests)
set_tests_properties(
    pae.tools.protocol_lab.c3_policy
    PROPERTIES
        LABELS "tools;protocol_lab;offline;decimal;dec042b;c3;policy;isolated"
        TIMEOUT 30
)

if(WIN32)
    find_program(pae_protocol_lab_c3_unicode_shell NAMES pwsh powershell REQUIRED)
    file(
        RELATIVE_PATH
        pae_protocol_lab_c3_binary_relative
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}"
    )
    add_test(
        NAME pae.tools.protocol_lab.c3_unicode_paths
        COMMAND
            "${pae_protocol_lab_c3_unicode_shell}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File tests/protocol_lab_c3_cli/verify_c3_unicode_paths.ps1
            -BinaryDirRelative "${pae_protocol_lab_c3_binary_relative}"
    )
    set_tests_properties(
        pae.tools.protocol_lab.c3_unicode_paths
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            LABELS "tools;protocol_lab;offline;decimal;dec042b;c3;cli;unicode;windows"
            TIMEOUT 180
    )
endif()
