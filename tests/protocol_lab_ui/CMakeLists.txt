set(
    pae_protocol_lab_ui_headless_test_names
        description_mapping
        compile_queue
        document_state
        dual_tab_isolation
        mailbox_ownership
        inspect_state
        bounded_v08_state
)

foreach(pae_ui_test_name IN LISTS pae_protocol_lab_ui_headless_test_names)
    set(pae_ui_test_target "pae_protocol_lab_ui_${pae_ui_test_name}_tests")
    add_executable("${pae_ui_test_target}" "${pae_ui_test_name}_tests.cpp")
    target_link_libraries(
        "${pae_ui_test_target}"
        PRIVATE
            pae_protocol_lab_ui_headless_internal
            pae::project_options
    )
    target_compile_definitions(
        "${pae_ui_test_target}"
        PRIVATE
            "PAE_UI_FIXTURE_DIR=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures\""
    )
    set_target_properties(
        "${pae_ui_test_target}"
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
    add_test(NAME "pae.tools.protocol_lab_ui.${pae_ui_test_name}" COMMAND "${pae_ui_test_target}")
    set_tests_properties(
        "pae.tools.protocol_lab_ui.${pae_ui_test_name}"
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;headless"
            TIMEOUT 180
    )
endforeach()

add_test(
    NAME pae.tools.protocol_lab_ui.qt_smoke
    COMMAND
        "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
        --ui-smoke
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v05.pae.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v06.pae.json"
)
set_tests_properties(
    pae.tools.protocol_lab_ui.qt_smoke
    PROPERTIES
        LABELS "tools;protocol_lab_ui;offline;qt;window;smoke"
        TIMEOUT 180
)

add_test(
    NAME pae.tools.protocol_lab_ui.qt_smoke_v08
    COMMAND
        "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
        --ui-smoke
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v08.pae.json"
)
set_tests_properties(
    pae.tools.protocol_lab_ui.qt_smoke_v08
    PROPERTIES
        LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v08"
        TIMEOUT 180
)

add_test(
    NAME pae.tools.protocol_lab_ui.qt_smoke_v07
    COMMAND
        "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
        --ui-smoke
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v07.pae.json"
)
set_tests_properties(
    pae.tools.protocol_lab_ui.qt_smoke_v07
    PROPERTIES
        LABELS "tools;protocol_lab_ui;offline;qt;window;smoke"
        TIMEOUT 180
)
