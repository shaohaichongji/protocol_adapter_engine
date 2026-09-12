include_guard(GLOBAL)

set(
    PAE_QT_ROOT
    ""
    CACHE PATH
    "External Qt 5.13 root used only by the optional Protocol Lab UI"
)

function(pae_require_protocol_lab_ui_toolchain)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_UI requires Windows and MSVC.")
    endif()
    if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio ")
        message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_UI requires a Visual Studio generator.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_UI requires an x64 generator platform.")
    endif()
    if(NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v142")
        message(
            FATAL_ERROR
            "PAE_BUILD_PROTOCOL_LAB_UI requires -T v142,version=14.29.30133; "
            "CMAKE_VS_PLATFORM_TOOLSET='${CMAKE_VS_PLATFORM_TOOLSET}'."
        )
    endif()
    if(NOT CMAKE_GENERATOR_TOOLSET MATCHES "(^|,)version=14\\.29\\.30133($|,)")
        message(
            FATAL_ERROR
            "PAE_BUILD_PROTOCOL_LAB_UI requires the explicit exact toolset request "
            "-T v142,version=14.29.30133; CMAKE_GENERATOR_TOOLSET="
            "'${CMAKE_GENERATOR_TOOLSET}'."
        )
    endif()
    cmake_path(NORMAL_PATH CMAKE_CXX_COMPILER OUTPUT_VARIABLE pae_ui_cxx_compiler)
    string(TOLOWER "${pae_ui_cxx_compiler}" pae_ui_cxx_compiler_lower)
    if(NOT pae_ui_cxx_compiler_lower MATCHES "[/\\\\]14\\.29\\.30133[/\\\\].*[/\\\\]cl\\.exe$")
        message(
            FATAL_ERROR
            "PAE_BUILD_PROTOCOL_LAB_UI requires cl.exe from the 14.29.30133 toolset "
            "directory; CMAKE_CXX_COMPILER='${pae_ui_cxx_compiler}'."
        )
    endif()
    if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "19.29.30159.0")
        message(
            FATAL_ERROR
            "The validated 14.29.30133 toolset must report cl compiler version "
            "19.29.30159.0; CMake reports '${CMAKE_CXX_COMPILER_VERSION}'."
        )
    endif()
    message(
        STATUS
        "Protocol Lab UI toolchain: requested v142 directory version 14.29.30133; "
        "MSBuild props mapping ${CMAKE_VS_PLATFORM_TOOLSET_VERSION}; cl compiler version "
        "${CMAKE_CXX_COMPILER_VERSION}; compiler ${pae_ui_cxx_compiler}"
    )
endfunction()

function(pae_import_qt513_widgets)
    if(NOT PAE_QT_ROOT)
        message(FATAL_ERROR "PAE_BUILD_PROTOCOL_LAB_UI requires -DPAE_QT_ROOT=<Qt 5.13 root>.")
    endif()

    cmake_path(ABSOLUTE_PATH PAE_QT_ROOT NORMALIZE OUTPUT_VARIABLE pae_qt_root)
    set(pae_qt_config "${pae_qt_root}/include/QtCore/qconfig.h")
    if(NOT EXISTS "${pae_qt_config}")
        message(FATAL_ERROR "PAE_QT_ROOT does not contain include/QtCore/qconfig.h.")
    endif()
    file(STRINGS "${pae_qt_config}" pae_qt_version_line REGEX "^#define QT_VERSION_STR ")
    if(NOT pae_qt_version_line MATCHES "\"(5\\.13\\.[0-9]+)\"")
        message(
            FATAL_ERROR
            "PAE_QT_ROOT must provide Qt 5.13.x; qconfig.h reports '${pae_qt_version_line}'."
        )
    endif()
    set(pae_qt_version "${CMAKE_MATCH_1}")

    foreach(pae_qt_module IN ITEMS Core Gui Widgets)
        foreach(pae_qt_config_name IN ITEMS debug release)
            if(pae_qt_config_name STREQUAL "debug")
                set(pae_qt_suffix "d")
                string(TOUPPER "${pae_qt_config_name}" pae_qt_import_config)
            else()
                set(pae_qt_suffix "")
                string(TOUPPER "${pae_qt_config_name}" pae_qt_import_config)
            endif()
            set(pae_qt_implib "${pae_qt_root}/lib/${pae_qt_config_name}/Qt5${pae_qt_module}${pae_qt_suffix}.lib")
            set(pae_qt_dll "${pae_qt_root}/bin/${pae_qt_config_name}/Qt5${pae_qt_module}${pae_qt_suffix}.dll")
            if(NOT EXISTS "${pae_qt_implib}")
                message(FATAL_ERROR "Missing Qt import library: ${pae_qt_implib}")
            endif()
            if(NOT EXISTS "${pae_qt_dll}")
                message(FATAL_ERROR "Missing Qt runtime DLL: ${pae_qt_dll}")
            endif()
            set("pae_qt_${pae_qt_module}_${pae_qt_import_config}_implib" "${pae_qt_implib}")
            set("pae_qt_${pae_qt_module}_${pae_qt_import_config}_dll" "${pae_qt_dll}")
        endforeach()

        if(NOT EXISTS "${pae_qt_root}/include/Qt${pae_qt_module}")
            message(FATAL_ERROR "Missing Qt module include directory: include/Qt${pae_qt_module}")
        endif()

        add_library("PAE_Qt5${pae_qt_module}" SHARED IMPORTED GLOBAL)
        add_library("PAE::Qt5${pae_qt_module}" ALIAS "PAE_Qt5${pae_qt_module}")
        set_target_properties(
            "PAE_Qt5${pae_qt_module}"
            PROPERTIES
                IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
                IMPORTED_IMPLIB_DEBUG "${pae_qt_${pae_qt_module}_DEBUG_implib}"
                IMPORTED_LOCATION_DEBUG "${pae_qt_${pae_qt_module}_DEBUG_dll}"
                IMPORTED_IMPLIB_RELEASE "${pae_qt_${pae_qt_module}_RELEASE_implib}"
                IMPORTED_LOCATION_RELEASE "${pae_qt_${pae_qt_module}_RELEASE_dll}"
                MAP_IMPORTED_CONFIG_MINSIZEREL Release
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
                INTERFACE_INCLUDE_DIRECTORIES "${pae_qt_root}/include;${pae_qt_root}/include/Qt${pae_qt_module}"
                INTERFACE_QT_MAJOR_VERSION 5
        )
    endforeach()

    set_property(TARGET PAE_Qt5Core APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_CORE_LIB)
    set_property(TARGET PAE_Qt5Gui APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_GUI_LIB)
    set_property(TARGET PAE_Qt5Gui APPEND PROPERTY INTERFACE_LINK_LIBRARIES PAE::Qt5Core)
    set_property(TARGET PAE_Qt5Widgets APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QT_WIDGETS_LIB)
    set_property(
        TARGET PAE_Qt5Widgets
        APPEND PROPERTY INTERFACE_LINK_LIBRARIES PAE::Qt5Gui PAE::Qt5Core
    )

    foreach(pae_qt_tool IN ITEMS moc rcc uic)
        if(NOT EXISTS "${pae_qt_root}/bin/${pae_qt_tool}.exe")
            message(FATAL_ERROR "Missing Qt build tool: bin/${pae_qt_tool}.exe")
        endif()
    endforeach()
    foreach(pae_qt_platform IN ITEMS debug/platforms/qwindowsd.dll release/platforms/qwindows.dll)
        if(NOT EXISTS "${pae_qt_root}/bin/${pae_qt_platform}")
            message(FATAL_ERROR "Missing Qt platform plugin: bin/${pae_qt_platform}")
        endif()
    endforeach()

    set(PAE_QT_VERSION "${pae_qt_version}" CACHE INTERNAL "Validated Protocol Lab UI Qt version")
    set(PAE_QT_MOC_EXECUTABLE "${pae_qt_root}/bin/moc.exe" CACHE INTERNAL "Validated Qt moc")
    set(PAE_QT_RCC_EXECUTABLE "${pae_qt_root}/bin/rcc.exe" CACHE INTERNAL "Validated Qt rcc")
    set(PAE_QT_UIC_EXECUTABLE "${pae_qt_root}/bin/uic.exe" CACHE INTERNAL "Validated Qt uic")
    set(Qt5Core_VERSION_MAJOR 5 PARENT_SCOPE)
    set(Qt5Core_VERSION_MINOR 13 PARENT_SCOPE)
    message(STATUS "Protocol Lab UI Qt input: ${pae_qt_version} at ${pae_qt_root}")
endfunction()

function(pae_configure_protocol_lab_ui_deployment target_name)
    set(options)
    set(one_value_args)
    set(multi_value_args CONFIG_FILES)
    cmake_parse_arguments(PAE_DEPLOY "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if(PAE_DEPLOY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unexpected deployment arguments: ${PAE_DEPLOY_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Unknown Protocol Lab UI deployment target: ${target_name}")
    endif()
    if(NOT PAE_DEPLOY_CONFIG_FILES)
        message(FATAL_ERROR "Protocol Lab UI deployment requires explicit public synthetic configs.")
    endif()

    set(pae_deploy_config_paths)
    foreach(pae_deploy_config IN LISTS PAE_DEPLOY_CONFIG_FILES)
        cmake_path(ABSOLUTE_PATH pae_deploy_config BASE_DIRECTORY "${PROJECT_SOURCE_DIR}" NORMALIZE)
        if(NOT EXISTS "${pae_deploy_config}")
            message(FATAL_ERROR "Missing Protocol Lab UI deployment config: ${pae_deploy_config}")
        endif()
        if(NOT pae_deploy_config MATCHES "[/\\\\]tests[/\\\\]protocol_lab_ui[/\\\\]fixtures[/\\\\].+\\.pae\\.json$" AND
           NOT pae_deploy_config STREQUAL
               "${PROJECT_BINARY_DIR}/generated/protocol_lab_ui/synthetic_ui_max.pae.json" AND
           NOT pae_deploy_config STREQUAL
               "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json" AND
           NOT pae_deploy_config STREQUAL
               "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_literal_only.pae.json")
            message(
                FATAL_ERROR
                "Only public synthetic tests/protocol_lab_ui/fixtures/*.pae.json files or the "
                "generated protocol_lab_ui maximum fixture may be deployed: "
                "${pae_deploy_config}"
            )
        endif()
        list(APPEND pae_deploy_config_paths "${pae_deploy_config}")
    endforeach()
    string(JOIN "|" pae_deploy_config_arg ${pae_deploy_config_paths})

    set_target_properties(
        "${target_name}"
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>"
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
            AUTOMOC_EXECUTABLE "${PAE_QT_MOC_EXECUTABLE}"
            AUTORCC_EXECUTABLE "${PAE_QT_RCC_EXECUTABLE}"
            AUTOUIC_EXECUTABLE "${PAE_QT_UIC_EXECUTABLE}"
    )
    add_custom_command(
        TARGET "${target_name}"
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPAE_UI_EXE=$<TARGET_FILE:${target_name}>"
            "-DPAE_QT_ROOT=${PAE_QT_ROOT}"
            "-DPAE_BUILD_CONFIG=$<CONFIG>"
            "-DPAE_DEPLOY_DIR=${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>"
            "-DPAE_ALLOWED_OUT_ROOT=${PROJECT_BINARY_DIR}/out/protocol_lab_ui"
            "-DPAE_CONFIG_FILES=${pae_deploy_config_arg}"
            -P "${PROJECT_SOURCE_DIR}/cmake/DeployProtocolLabUi.cmake"
        VERBATIM
        COMMENT "Deploying the offline Protocol Lab UI whitelist"
    )
endfunction()
