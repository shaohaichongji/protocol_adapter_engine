set(
    pae_protocol_lab_ui_headless_test_names
        description_mapping
        compile_queue
        document_state
        dual_tab_isolation
        mailbox_ownership
        inspect_state
        bounded_v08_state
        schema_dispatch
)

if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_binary_stage1
        COMMAND
            "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
            --ui-smoke
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.qt_smoke_binary_stage1
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v09;binary;host"
            TIMEOUT 180
    )
endif()

if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    list(APPEND pae_protocol_lab_ui_headless_test_names ascii_input ascii_session)
    add_executable(pae_protocol_lab_ui_ascii_smoke_editor_tests ascii_smoke_editor_tests.cpp)
    target_include_directories(pae_protocol_lab_ui_ascii_smoke_editor_tests PRIVATE "${PROJECT_SOURCE_DIR}/tools/protocol_lab_ui")
    target_link_libraries(pae_protocol_lab_ui_ascii_smoke_editor_tests PRIVATE PAE::Qt5Widgets pae::project_options)
    set(pae_ascii_smoke_qt_config "$<IF:$<CONFIG:Debug>,debug,release>")
    set(pae_ascii_smoke_qt_suffix "$<IF:$<CONFIG:Debug>,d,>")
    add_custom_command(TARGET pae_protocol_lab_ui_ascii_smoke_editor_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:pae_protocol_lab_ui_ascii_smoke_editor_tests>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${PAE_QT_ROOT}/bin/${pae_ascii_smoke_qt_config}/Qt5Core${pae_ascii_smoke_qt_suffix}.dll"
            "${PAE_QT_ROOT}/bin/${pae_ascii_smoke_qt_config}/Qt5Gui${pae_ascii_smoke_qt_suffix}.dll"
            "${PAE_QT_ROOT}/bin/${pae_ascii_smoke_qt_config}/Qt5Widgets${pae_ascii_smoke_qt_suffix}.dll"
            "$<TARGET_FILE_DIR:pae_protocol_lab_ui_ascii_smoke_editor_tests>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${PAE_QT_ROOT}/bin/${pae_ascii_smoke_qt_config}/platforms/qwindows${pae_ascii_smoke_qt_suffix}.dll"
            "$<TARGET_FILE_DIR:pae_protocol_lab_ui_ascii_smoke_editor_tests>/platforms"
        VERBATIM)
    if(MSVC)
        target_compile_options(pae_protocol_lab_ui_ascii_smoke_editor_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(pae_protocol_lab_ui_ascii_smoke_editor_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pae.tools.protocol_lab_ui.ascii_smoke_editor COMMAND pae_protocol_lab_ui_ascii_smoke_editor_tests)
    set_tests_properties(pae.tools.protocol_lab_ui.ascii_smoke_editor PROPERTIES LABELS "tools;protocol_lab_ui;offline;qt;ascii;smoke" TIMEOUT 180)
endif()

if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_ascii_stream
        COMMAND
            "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
            --ui-smoke
            "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.qt_smoke_ascii_stream
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v11;ascii;stream"
            TIMEOUT 180
    )
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_ascii_stream_close_cancel
        COMMAND
            "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
            --ui-smoke
            "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
            "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.qt_smoke_ascii_stream_close_cancel
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v11;ascii;stream;close"
            TIMEOUT 180
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    list(APPEND pae_protocol_lab_ui_headless_test_names ascii_stream_session)
endif()
if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    list(APPEND pae_protocol_lab_ui_headless_test_names host_session)
endif()
if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI AND NOT PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    list(APPEND pae_protocol_lab_ui_headless_test_names binary_stage1)
endif()
if(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    list(APPEND pae_protocol_lab_ui_headless_test_names binary_public_h2 binary_public_header)
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    list(APPEND pae_protocol_lab_ui_headless_test_names ascii_public_a2)
endif()

foreach(pae_ui_test_name IN LISTS pae_protocol_lab_ui_headless_test_names)
    set(pae_ui_test_target "pae_protocol_lab_ui_${pae_ui_test_name}_tests")
    add_executable("${pae_ui_test_target}" "${pae_ui_test_name}_tests.cpp")
    # These tests use assert for both checks and setup; Release must execute them too.
    if(MSVC)
        target_compile_options("${pae_ui_test_target}" PRIVATE /UNDEBUG)
    else()
        target_compile_options("${pae_ui_test_target}" PRIVATE -UNDEBUG)
    endif()
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
    if(pae_ui_test_name STREQUAL "ascii_session")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_TEXT_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json\""
                "PAE_ASCII_LITERAL_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_literal_only.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "ascii_public_a2")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_TEXT_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "ascii_stream_session" OR pae_ui_test_name STREQUAL "host_session")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_STREAM_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json\""
        )
    endif()
    if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI AND pae_ui_test_name STREQUAL "compile_queue")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_STREAM_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "binary_stage1")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_BINARY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json\""
                "PAE_BINARY_STREAM_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "binary_public_h2")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_BINARY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json\""
                "PAE_LEGACY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v05.pae.json\""
                "PAE_ASCII_TEXT_CONFIG=L\"${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json\""
        )
    endif()
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

if(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_ascii
        COMMAND
            "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
            --ui-smoke
            "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
            "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_literal_only.pae.json"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.qt_smoke_ascii
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v10;ascii"
            TIMEOUT 180
    )
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_ascii_one_way
        COMMAND
            "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
            --ui-smoke
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ascii_decode_only.pae.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ascii_encode_only.pae.json"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.qt_smoke_ascii_one_way
        PROPERTIES
            LABELS "tools;protocol_lab_ui;offline;qt;window;smoke;schema_v10;ascii;one_way"
            TIMEOUT 180
    )
endif()
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
