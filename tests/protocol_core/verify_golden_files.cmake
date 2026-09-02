if(NOT DEFINED PAE_TEST_EXECUTABLE OR PAE_TEST_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "PAE_TEST_EXECUTABLE is required")
endif()
if(NOT EXISTS "${PAE_TEST_EXECUTABLE}")
    message(FATAL_ERROR "Protocol Core contract test executable is missing: ${PAE_TEST_EXECUTABLE}")
endif()
if(NOT DEFINED PAE_TEST_DATA_DIR OR PAE_TEST_DATA_DIR STREQUAL "")
    message(FATAL_ERROR "PAE_TEST_DATA_DIR is required")
endif()

set(
    pae_runtime_golden_dir
    "${PAE_TEST_DATA_DIR}/golden/synthetic_lab_exchange"
)

function(pae_verify_runtime_sha256 relative_path expected_sha256)
    set(file_path "${pae_runtime_golden_dir}/${relative_path}")
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Synthetic Engine Vector runtime copy is missing: ${file_path}")
    endif()

    file(SHA256 "${file_path}" actual_sha256)
    string(TOUPPER "${actual_sha256}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Synthetic Engine Vector runtime SHA-256 mismatch: ${file_path}\n"
            "expected=${expected_sha256}\n"
            "actual=${actual_sha256}"
        )
    endif()
endfunction()

pae_verify_runtime_sha256(
    "lab_command_001.frame.hex"
    "492AF67348459F1450FC87287B5CC6907D6FF5E75A295DBA36B489E09909040D"
)
pae_verify_runtime_sha256(
    "lab_command_001.decode_expected.tsv"
    "55F98CD3BF4DA09D0228781AE7F24E9843B9DA0F10FCAE04CEAF268D974B641F"
)
pae_verify_runtime_sha256(
    "lab_command_001.encode_input.tsv"
    "714D63AC19391AA8F52A31EF0BB6C7B84291FAA4BCF723B096DC202F06856DD3"
)
pae_verify_runtime_sha256(
    "lab_report_001.frame.hex"
    "AB2C014349E0C673FBF38C692DC2B5F1CB70609CF00968D94449A848E3B24E31"
)
pae_verify_runtime_sha256(
    "lab_report_001.decode_expected.tsv"
    "3734FA466D7412FF007880873FC9F9A5AACB4513A0837B3D36A4AD14D25E8CE5"
)
pae_verify_runtime_sha256(
    "lab_report_001.encode_input.tsv"
    "69BCD63709A6236C120B7072D28040035BCC3EF43B13C8A2F15E248216681246"
)

execute_process(
    COMMAND "${PAE_TEST_EXECUTABLE}" "${PAE_TEST_DATA_DIR}"
    RESULT_VARIABLE pae_test_result
)
if(NOT pae_test_result STREQUAL "0")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29" AND pae_test_result MATCHES "^[0-9]+$")
        if(pae_test_result LESS_EQUAL 125)
            cmake_language(EXIT "${pae_test_result}")
        endif()
    endif()
    message(FATAL_ERROR "Protocol Core contract test exited with code: ${pae_test_result}")
endif()
