set(pae_lab_ui_test_source_root "${PROJECT_SOURCE_DIR}")
set(pae_lab_ui_headless_test_target pae_protocol_lab_ui_headless_internal)
set(
    pae_lab_ui_smoke_executable
    "${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe"
)
if(DEFINED PAE_LAB_STANDALONE_TEST_SOURCE_ROOT)
    set(pae_lab_ui_test_source_root "${PAE_LAB_STANDALONE_TEST_SOURCE_ROOT}")
    set(pae_lab_ui_headless_test_target "${PAE_PROTOCOL_LAB_UI_HEADLESS_TEST_TARGET}")
    set(pae_lab_ui_smoke_executable "${PAE_LAB_UI_SMOKE_EXECUTABLE}")
endif()

if(MSVC)
    add_test(
        NAME pae.tools.protocol_lab_ui.ui_smoke_crt_assert_probe
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPAE_SMOKE_EXECUTABLE=${pae_lab_ui_smoke_executable}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_crt_assert_probe.cmake"
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.ui_smoke_crt_assert_probe
        PROPERTIES
            CONFIGURATIONS Debug
            LABELS "tools;protocol_lab_ui;offline;diagnostic;smoke;crt"
            TIMEOUT 30
    )
endif()

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

add_executable(
    pae_protocol_lab_ui_owned_presentation_types_tests
    owned_presentation_types_tests.cpp
)
if(MSVC)
    target_compile_options(pae_protocol_lab_ui_owned_presentation_types_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(pae_protocol_lab_ui_owned_presentation_types_tests PRIVATE -UNDEBUG)
endif()
target_include_directories(
    pae_protocol_lab_ui_owned_presentation_types_tests
    PRIVATE "${pae_lab_ui_test_source_root}/tools/protocol_lab_ui"
)
target_link_libraries(
    pae_protocol_lab_ui_owned_presentation_types_tests
    PRIVATE
        pae_protocol_lab_ui_owned_presentation
        pae::project_options
)
add_test(
    NAME pae.tools.protocol_lab_ui.owned_presentation_types
    COMMAND pae_protocol_lab_ui_owned_presentation_types_tests
)
set_tests_properties(
    pae.tools.protocol_lab_ui.owned_presentation_types
    PROPERTIES LABELS "tools;protocol_lab_ui;owned_types;offline" TIMEOUT 60
)

if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_binary_stage1
        COMMAND
            "${pae_lab_ui_smoke_executable}"
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

if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
    list(APPEND pae_protocol_lab_ui_headless_test_names ascii_input ascii_session)
    add_executable(pae_protocol_lab_ui_ascii_smoke_editor_tests ascii_smoke_editor_tests.cpp)
    target_include_directories(pae_protocol_lab_ui_ascii_smoke_editor_tests PRIVATE "${pae_lab_ui_test_source_root}/tools/protocol_lab_ui")
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
            "${pae_lab_ui_smoke_executable}"
            --ui-smoke
            "${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_stream_slice.pae.json"
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
            "${pae_lab_ui_smoke_executable}"
            --ui-smoke
            "${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_stream_slice.pae.json"
            "${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_stream_slice.pae.json"
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
    add_executable(
        pae_protocol_lab_ui_binary_stream_qt_smoke_tests
        binary_stream_qt_smoke_tests.cpp
        ${pae_lab_ui_test_source_root}/tools/protocol_lab_ui/document_tab.cpp
        ${pae_lab_ui_test_source_root}/tools/protocol_lab_ui/exact_value_delegate.cpp
        ${pae_lab_ui_test_source_root}/tools/protocol_lab_ui/field_table_model.cpp
        ${pae_lab_ui_test_source_root}/tools/protocol_lab_ui/hex_view.cpp
    )
    target_include_directories(
        pae_protocol_lab_ui_binary_stream_qt_smoke_tests
        PRIVATE "${pae_lab_ui_test_source_root}/tools/protocol_lab_ui"
    )
    target_link_libraries(
        pae_protocol_lab_ui_binary_stream_qt_smoke_tests
        PRIVATE pae_protocol_lab_ui_headless_internal PAE::Qt5Widgets pae::project_options
    )
    target_compile_definitions(
        pae_protocol_lab_ui_binary_stream_qt_smoke_tests
        PRIVATE
            "PAE_BINARY_STREAM_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_stream_framing_slice.pae.json\""
    )
    if(MSVC)
        target_compile_options(pae_protocol_lab_ui_binary_stream_qt_smoke_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(pae_protocol_lab_ui_binary_stream_qt_smoke_tests PRIVATE -UNDEBUG)
    endif()
    set(pae_binary_stream_qt_config "$<IF:$<CONFIG:Debug>,debug,release>")
    set(pae_binary_stream_qt_suffix "$<IF:$<CONFIG:Debug>,d,>")
    add_custom_command(
        TARGET pae_protocol_lab_ui_binary_stream_qt_smoke_tests POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:pae_protocol_lab_ui_binary_stream_qt_smoke_tests>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${PAE_QT_ROOT}/bin/${pae_binary_stream_qt_config}/Qt5Core${pae_binary_stream_qt_suffix}.dll"
            "${PAE_QT_ROOT}/bin/${pae_binary_stream_qt_config}/Qt5Gui${pae_binary_stream_qt_suffix}.dll"
            "${PAE_QT_ROOT}/bin/${pae_binary_stream_qt_config}/Qt5Widgets${pae_binary_stream_qt_suffix}.dll"
            "$<TARGET_FILE_DIR:pae_protocol_lab_ui_binary_stream_qt_smoke_tests>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${PAE_QT_ROOT}/bin/${pae_binary_stream_qt_config}/platforms/qwindows${pae_binary_stream_qt_suffix}.dll"
            "$<TARGET_FILE_DIR:pae_protocol_lab_ui_binary_stream_qt_smoke_tests>/platforms"
    )
    add_test(
        NAME pae.tools.protocol_lab_ui.binary_stream_qt_smoke
        COMMAND pae_protocol_lab_ui_binary_stream_qt_smoke_tests
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.binary_stream_qt_smoke
        PROPERTIES LABELS "tools;protocol_lab_ui;offline;qt;binary;stream;smoke" TIMEOUT 180
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    add_executable(
        pae_protocol_lab_ui_ascii_public_facade_isolation_tests
        ascii_public_facade_isolation_tests.cpp
    )
    if(MSVC)
        target_compile_options(
            pae_protocol_lab_ui_ascii_public_facade_isolation_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            pae_protocol_lab_ui_ascii_public_facade_isolation_tests PRIVATE -UNDEBUG
        )
    endif()
    target_link_libraries(
        pae_protocol_lab_ui_ascii_public_facade_isolation_tests
        PRIVATE pae_protocol_lab_ui_ascii_public_facade pae::project_options
    )
    target_compile_definitions(
        pae_protocol_lab_ui_ascii_public_facade_isolation_tests
        PRIVATE
            "PAE_ASCII_TEXT_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_text_slice.pae.json\""
    )
    add_test(
        NAME pae.tools.protocol_lab_ui.ascii_public_facade_isolation
        COMMAND pae_protocol_lab_ui_ascii_public_facade_isolation_tests
    )
    set_tests_properties(
        pae.tools.protocol_lab_ui.ascii_public_facade_isolation
        PROPERTIES LABELS "tools;protocol_lab_ui;offline;ascii;public_facade" TIMEOUT 60
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2 AND PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    list(APPEND pae_protocol_lab_ui_headless_test_names ascii_public_a2)
endif()
if(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
    list(APPEND pae_protocol_lab_ui_headless_test_names public_legacy_complete)
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
            ${pae_lab_ui_headless_test_target}
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
                "PAE_ASCII_TEXT_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_text_slice.pae.json\""
                "PAE_ASCII_LITERAL_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_literal_only.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "ascii_public_a2")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_TEXT_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_text_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "ascii_stream_session" OR pae_ui_test_name STREQUAL "host_session")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_STREAM_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_stream_slice.pae.json\""
        )
    endif()
    if(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI AND pae_ui_test_name STREQUAL "compile_queue")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_ASCII_STREAM_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_stream_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "binary_stage1")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_BINARY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json\""
                "PAE_BINARY_STREAM_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_stream_framing_slice.pae.json\""
        )
    endif()
    if(pae_ui_test_name STREQUAL "binary_public_h2")
        target_compile_definitions(
            "${pae_ui_test_target}"
            PRIVATE
                "PAE_BINARY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_binary_ui_stage1.pae.json\""
                "PAE_LEGACY_UI_CONFIG=L\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v05.pae.json\""
                "PAE_ASCII_TEXT_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_text_slice.pae.json\""
                "PAE_BINARY_STREAM_CONFIG=L\"${pae_lab_ui_test_source_root}/examples/config/synthetic_stream_framing_slice.pae.json\""
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
        "${pae_lab_ui_smoke_executable}"
        --ui-smoke
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v05.pae.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v06.pae.json"
)

if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
    add_test(
        NAME pae.tools.protocol_lab_ui.qt_smoke_ascii
        COMMAND
            "${pae_lab_ui_smoke_executable}"
            --ui-smoke
            "${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_text_slice.pae.json"
            "${pae_lab_ui_test_source_root}/examples/config/synthetic_ascii_literal_only.pae.json"
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
            "${pae_lab_ui_smoke_executable}"
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
        "${pae_lab_ui_smoke_executable}"
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
        "${pae_lab_ui_smoke_executable}"
        --ui-smoke
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic_ui_v07.pae.json"
)
set_tests_properties(
    pae.tools.protocol_lab_ui.qt_smoke_v07
    PROPERTIES
        LABELS "tools;protocol_lab_ui;offline;qt;window;smoke"
        TIMEOUT 180
)

set(
    pae_protocol_lab_ui_qwindows_smoke_tests
        pae.tools.protocol_lab_ui.qt_smoke
        pae.tools.protocol_lab_ui.qt_smoke_v07
        pae.tools.protocol_lab_ui.qt_smoke_v08
)
if(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    list(APPEND pae_protocol_lab_ui_qwindows_smoke_tests pae.tools.protocol_lab_ui.qt_smoke_binary_stage1)
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    list(
        APPEND pae_protocol_lab_ui_qwindows_smoke_tests
        pae.tools.protocol_lab_ui.qt_smoke_ascii_stream
        pae.tools.protocol_lab_ui.qt_smoke_ascii_stream_close_cancel
    )
endif()
if(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
    list(
        APPEND pae_protocol_lab_ui_qwindows_smoke_tests
        pae.tools.protocol_lab_ui.qt_smoke_ascii
        pae.tools.protocol_lab_ui.qt_smoke_ascii_one_way
    )
endif()
foreach(pae_protocol_lab_ui_qwindows_smoke_test IN LISTS pae_protocol_lab_ui_qwindows_smoke_tests)
    set_property(
        TEST "${pae_protocol_lab_ui_qwindows_smoke_test}"
        APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=windows"
    )
endforeach()
