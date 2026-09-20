if(NOT DEFINED PAE_SMOKE_EXECUTABLE OR PAE_SMOKE_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "PAE_SMOKE_EXECUTABLE is required")
endif()

execute_process(
    COMMAND "${PAE_SMOKE_EXECUTABLE}" --ui-smoke --ui-smoke-crt-assert-probe
    RESULT_VARIABLE pae_probe_result
    OUTPUT_VARIABLE pae_probe_stdout
    ERROR_VARIABLE pae_probe_stderr
    TIMEOUT 15
)

message(STATUS "CRT assert probe result: ${pae_probe_result}")
message(STATUS "CRT assert probe stderr:\n${pae_probe_stderr}")

if(pae_probe_result STREQUAL "0")
    message(FATAL_ERROR "CRT assert probe unexpectedly returned success")
endif()
if(NOT pae_probe_stderr MATCHES "UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled")
    message(FATAL_ERROR "CRT assert probe is missing the smoke diagnostic marker")
endif()
if(NOT pae_probe_stderr MATCHES "UI_SMOKE_CRT_FAIL_FAST")
    message(FATAL_ERROR "CRT assert probe is missing the fail-fast marker")
endif()
if(NOT pae_probe_stderr MATCHES "PAE_UI_SMOKE_CRT_ASSERT_PROBE")
    message(FATAL_ERROR "CRT assert probe is missing the assertion expression")
endif()
if(pae_probe_stderr MATCHES "UI_SMOKE_CRT_ASSERT_CONTINUED")
    message(FATAL_ERROR "CRT assertion returned to the caller instead of terminating")
endif()
