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
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_sum8_slice.pae.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_sum8_slice.pae.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_sum8_slice.values.pae-lab.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_sum8_slice.values.pae-lab.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_int64_slice.pae.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_int64_slice.pae.json"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/examples/config/synthetic_int64_slice.values.pae-lab.json"
    "${pae_protocol_lab_test_data_dir}/synthetic_int64_slice.values.pae-lab.json"
    COPYONLY
)
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
    configure_file(
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_crc_slice.pae.json"
        "${pae_protocol_lab_test_data_dir}/synthetic_crc_slice.pae.json"
        COPYONLY
    )
    configure_file(
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_crc_slice.values.pae-lab.json"
        "${pae_protocol_lab_test_data_dir}/synthetic_crc_slice.values.pae-lab.json"
        COPYONLY
    )
endif()
if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
    configure_file(
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json"
        "${pae_protocol_lab_test_data_dir}/synthetic_length_slice.pae.json"
        COPYONLY
    )
endif()
configure_file(
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden/synthetic_int64/int64_record_001.frame.hex"
    "${pae_protocol_lab_test_data_dir}/int64_record_001.frame.hex"
    COPYONLY
)
configure_file(
    "${PROJECT_SOURCE_DIR}/tests/protocol_core/golden/synthetic_sum8/sum8_record_001.frame.hex"
    "${pae_protocol_lab_test_data_dir}/sum8_record_001.frame.hex"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sum8_cross_pipeline_ambiguous.pae.json"
    "${pae_protocol_lab_test_data_dir}/sum8_cross_pipeline_ambiguous.pae.json"
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
    NAME pae.tools.protocol_lab.sum8_generation_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_LAB_DATA_DIR=${pae_protocol_lab_test_data_dir}"
        "-DPAE_LAB_RUN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sum8-runs-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_sum8_generation_contract.cmake"
)
add_test(
    NAME pae.tools.protocol_lab.int64_generation_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPAE_LAB_EXECUTABLE=$<TARGET_FILE:pae_protocol_lab>"
        "-DPAE_LAB_DATA_DIR=${pae_protocol_lab_test_data_dir}"
        "-DPAE_LAB_RUN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/int64-runs-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_int64_generation_contract.cmake"
)
set_tests_properties(
    pae.tools.protocol_lab.int64_generation_contract
    PROPERTIES LABELS "tools;protocol_lab;offline;int64;dec042a" TIMEOUT 180
               WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
set_tests_properties(
    pae.tools.protocol_lab.sum8_generation_contract
    PROPERTIES LABELS "tools;protocol_lab;offline;sum8;dec041" TIMEOUT 180
               WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(
    pae_protocol_lab_inspect_phase_tests
    inspect_frame_phase_tests.cpp
)
target_include_directories(
    pae_protocol_lab_inspect_phase_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/config_compiler"
        "${PROJECT_SOURCE_DIR}/src/protocol_core"
        "${PROJECT_SOURCE_DIR}/src/protocol_plan"
        "${PROJECT_SOURCE_DIR}/tools/protocol_lab"
)
target_link_libraries(
    pae_protocol_lab_inspect_phase_tests
    PRIVATE
        pae_protocol_lab_internal
        pae_config_compiler
        pae::project_options
        pae::yyjson
)
add_test(
    NAME pae.tools.protocol_lab.inspect_phase
    COMMAND pae_protocol_lab_inspect_phase_tests "${pae_protocol_lab_test_data_dir}"
)
set_tests_properties(
    pae.tools.protocol_lab.inspect_phase
    PROPERTIES LABELS "tools;protocol_lab;offline;sum8;dec041" TIMEOUT 180
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

    add_test(
        NAME pae.tools.protocol_lab.sum8_schema_contract
        COMMAND
            "${pae_powershell_executable}"
            -NoProfile
            -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_sum8_schema_contract.ps1"
            -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
            -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
            -ConfigPath "${pae_protocol_lab_test_data_dir}/synthetic_sum8_slice.pae.json"
            -ValuesPath "${pae_protocol_lab_test_data_dir}/synthetic_sum8_slice.values.pae-lab.json"
            -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/sum8-schema-$<CONFIG>"
    )
    add_test(
        NAME pae.tools.protocol_lab.int64_schema_contract
        COMMAND
            "${pae_powershell_executable}"
            -NoProfile
            -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_int64_schema_contract.ps1"
            -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
            -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
            -ConfigPath "${pae_protocol_lab_test_data_dir}/synthetic_int64_slice.pae.json"
            -ValuesPath "${pae_protocol_lab_test_data_dir}/synthetic_int64_slice.values.pae-lab.json"
            -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/int64-schema-$<CONFIG>"
    )
    set_tests_properties(
        pae.tools.protocol_lab.int64_schema_contract
        PROPERTIES LABELS "tools;protocol_lab;offline;int64;dec042a;schema" TIMEOUT 180
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_tests_properties(
        pae.tools.protocol_lab.sum8_schema_contract
        PROPERTIES LABELS "tools;protocol_lab;offline;sum8;dec041;schema" TIMEOUT 180
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
        add_test(
            NAME pae.tools.protocol_lab.crc_schema_contract
            COMMAND
                "${pae_powershell_executable}"
                -NoProfile
                -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_crc_schema_contract.ps1"
                -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
                -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
                -ConfigPath "${pae_protocol_lab_test_data_dir}/synthetic_crc_slice.pae.json"
                -ValuesPath "${pae_protocol_lab_test_data_dir}/synthetic_crc_slice.values.pae-lab.json"
                -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/crc-schema-$<CONFIG>"
        )
        set_tests_properties(
            pae.tools.protocol_lab.crc_schema_contract
            PROPERTIES LABELS "tools;protocol_lab;offline;crc;schema" TIMEOUT 180
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
    if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
        add_test(
            NAME pae.tools.protocol_lab.length_schema_contract
            COMMAND
                "${pae_powershell_executable}"
                -NoProfile
                -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_length_schema_contract.ps1"
                -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
                -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
                -ConfigPath "${PROJECT_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json"
                -ValuesPath "${PROJECT_SOURCE_DIR}/tests/protocol_lab_c3_cli/fixtures/length.values.pae-lab.json"
                -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/length-schema-$<CONFIG>"
        )
        set_tests_properties(
            pae.tools.protocol_lab.length_schema_contract
            PROPERTIES LABELS "tools;protocol_lab;offline;length_field;schema" TIMEOUT 180
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
    if(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE)
        add_test(
            NAME pae.tools.protocol_lab.variable_record_schema_contract
            COMMAND
                "${pae_powershell_executable}"
                -NoProfile
                -File "${CMAKE_CURRENT_SOURCE_DIR}/verify_variable_record_schema_contract.ps1"
                -LabExecutable "$<TARGET_FILE:pae_protocol_lab>"
                -SchemaPath "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
                -ConfigPath "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.pae.json"
                -ValuesPath "${PROJECT_SOURCE_DIR}/examples/config/synthetic_bounded_variable_record.values.pae-lab.json"
                -FixedConfigPath "${PROJECT_SOURCE_DIR}/examples/config/synthetic_length_slice.pae.json"
                -FixedValuesPath "${PROJECT_SOURCE_DIR}/tests/protocol_lab_c3_cli/fixtures/length.values.pae-lab.json"
                -TestRoot "${CMAKE_CURRENT_BINARY_DIR}/variable-record-schema-$<CONFIG>"
        )
        set_tests_properties(
            pae.tools.protocol_lab.variable_record_schema_contract
            PROPERTIES LABELS "tools;protocol_lab;offline;bounded_variable;schema" TIMEOUT 180
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()

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
    add_test(
        NAME pae.tools.protocol_lab.dec041_failure_chain
        COMMAND pae_protocol_lab_udp_exchange_tests --dec041-offline-only
    )
    set_tests_properties(
        pae.tools.protocol_lab.dec041_failure_chain
        PROPERTIES
            LABELS "tools;protocol_lab;offline;sum8;dec041"
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
