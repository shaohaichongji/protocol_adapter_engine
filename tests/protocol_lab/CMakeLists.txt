set(pae_protocol_lab_test_data_dir "${CMAKE_CURRENT_BINARY_DIR}/data")
file(MAKE_DIRECTORY "${pae_protocol_lab_test_data_dir}")

configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_lab_exchange_slice.pae.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bitfield_slice.pae.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_bitfield_slice.pae.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bitfield_slice.values.pae-lab.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_bitfield_slice.values.pae-lab.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden/synthetic_bitfield/bitfield_record_001.frame.hex"
    "${pae_protocol_lab_test_data_dir}/bitfield_record_001.frame.hex"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden/synthetic_lab_exchange/lab_command_001.frame.hex"
    "${pae_protocol_lab_test_data_dir}/lab_command_001.frame.hex"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden/synthetic_lab_exchange/lab_report_001.frame.hex"
    "${pae_protocol_lab_test_data_dir}/lab_report_001.frame.hex"
    COPYONLY
)
foreach(pae_protocol_lab_fixture IN ITEMS
        valid_command.values.pae-lab.json
        invalid_missing.values.pae-lab.json
        invalid_duplicate.values.pae-lab.json
        invalid_type.values.pae-lab.json
        invalid_overflow.values.pae-lab.json
        invalid_constant_override.values.pae-lab.json
        invalid_config.pae.json
        ambiguous.pae.json
        ambiguous.frame.hex
        invalid_syntax.frame.hex
        unknown.frame.hex)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${pae_protocol_lab_fixture}"
        "${pae_protocol_lab_test_data_dir}/${pae_protocol_lab_fixture}"
        COPYONLY
    )
endforeach()

file(
    COPY "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/legacy_offline_v0_1"
    DESTINATION "${pae_protocol_lab_test_data_dir}"
)

add_test(
    NAME pae.tools.protocol_lab.offline_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_LAB_DATA_DIR=${pae_protocol_lab_test_data_dir}"
        "-DPAE_LAB_RUN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runs-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_offline_contract.cmake"
)

set_tests_properties(
    pae.tools.protocol_lab.offline_contract
    PROPERTIES
        LABELS "tools;protocol_lab;offline"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME pae.tools.protocol_lab.bitfield_generation_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_LAB_DATA_DIR=${pae_protocol_lab_test_data_dir}"
        "-DPAE_LAB_RUN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/bitfield-runs-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_bitfield_generation_contract.cmake"
)
set_tests_properties(
    pae.tools.protocol_lab.bitfield_generation_contract
    PROPERTIES LABELS "tools;protocol_lab;offline;bitfield;dec040" TIMEOUT 180
               WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(WIN32)
    find_program(pae_powershell_executable NAMES pwsh pwsh.exe REQUIRED)

    add_test(
        NAME pae.tools.protocol_lab.bitfield_schema_contract
        COMMAND
            "${pae_powershell_executable}"
            -NoProfile
            -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_bitfield_schema_contract.ps1"
            -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
            -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
            -ConfigPath "${pae_protocol_lab_test_data_dir}/synthetic_bitfield_slice.pae.json"
            -ValuesPath "${pae_protocol_lab_test_data_dir}/synthetic_bitfield_slice.values.pae-lab.json"
            -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/bitfield-schema-$<CONFIG>"
    )
    set_tests_properties(
        pae.tools.protocol_lab.bitfield_schema_contract
        PROPERTIES LABELS "tools;protocol_lab;offline;bitfield;dec040;schema" TIMEOUT 180
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_executable(
        pae_protocol_lab_udp_exchange_tests
        udp_exchange_tests.cpp
    )

    target_include_directories(
        pae_protocol_lab_udp_exchange_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
    )

    target_link_libraries(
        pae_protocol_lab_udp_exchange_tests
        PRIVATE
            pae_protocol_lab_internal
            pae::project_options
            ws2_32
    )

    add_test(
        NAME pae.tools.protocol_lab.udp_exchange
        COMMAND pae_protocol_lab_udp_exchange_tests
    )

    set_tests_properties(
        pae.tools.protocol_lab.udp_exchange
        PROPERTIES
            LABELS "tools;protocol_lab;udp;loopback"
            TIMEOUT 180
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME pae.tools.protocol_lab.dec040_failure_chain
        COMMAND pae_protocol_lab_udp_exchange_tests --dec040-offline-only
    )
    set_tests_properties(
        pae.tools.protocol_lab.dec040_failure_chain
        PROPERTIES
            LABELS "tools;protocol_lab;offline;bitfield;dec040"
            TIMEOUT 180
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

add_executable(
    pae_protocol_lab_evidence_bundle_transaction_tests
    evidence_bundle_transaction_tests.cpp
)

target_include_directories(
    pae_protocol_lab_evidence_bundle_transaction_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)

target_link_libraries(
    pae_protocol_lab_evidence_bundle_transaction_tests
    PRIVATE
        pae_protocol_lab_internal
        pae::project_options
)

add_test(
    NAME pae.tools.protocol_lab.evidence_bundle_transaction
    COMMAND pae_protocol_lab_evidence_bundle_transaction_tests
)

set_tests_properties(
    pae.tools.protocol_lab.evidence_bundle_transaction
    PROPERTIES
        LABELS "tools;protocol_lab;offline;fault_injection"
        TIMEOUT 180
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
