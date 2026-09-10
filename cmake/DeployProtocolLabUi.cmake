cmake_minimum_required(VERSION 3.25)

foreach(
    pae_required_variable
    IN ITEMS
        PAE_UI_EXE
        PAE_QT_ROOT
        PAE_BUILD_CONFIG
        PAE_DEPLOY_DIR
        PAE_ALLOWED_OUT_ROOT
        PAE_CONFIG_FILES
)
    if(NOT DEFINED ${pae_required_variable} OR "${${pae_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "DeployProtocolLabUi.cmake requires ${pae_required_variable}.")
    endif()
endforeach()

cmake_path(NORMAL_PATH PAE_DEPLOY_DIR OUTPUT_VARIABLE pae_deploy_dir)
cmake_path(NORMAL_PATH PAE_ALLOWED_OUT_ROOT OUTPUT_VARIABLE pae_allowed_out_root)
cmake_path(IS_PREFIX pae_allowed_out_root "${pae_deploy_dir}" NORMALIZE pae_deploy_is_bounded)
if(NOT pae_deploy_is_bounded OR pae_deploy_dir STREQUAL pae_allowed_out_root)
    message(FATAL_ERROR "Refusing to replace unbounded deployment directory: ${pae_deploy_dir}")
endif()
if(NOT PAE_BUILD_CONFIG STREQUAL "Debug" AND NOT PAE_BUILD_CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Protocol Lab UI deployment supports only Debug and Release configurations.")
endif()
if(NOT EXISTS "${PAE_UI_EXE}")
    message(FATAL_ERROR "Protocol Lab UI executable does not exist: ${PAE_UI_EXE}")
endif()

if(PAE_BUILD_CONFIG STREQUAL "Debug")
    set(pae_qt_dir debug)
    set(pae_qt_suffix d)
else()
    set(pae_qt_dir release)
    set(pae_qt_suffix "")
endif()

set(
    pae_qt_runtime_files
    "${PAE_QT_ROOT}/bin/${pae_qt_dir}/Qt5Core${pae_qt_suffix}.dll"
    "${PAE_QT_ROOT}/bin/${pae_qt_dir}/Qt5Gui${pae_qt_suffix}.dll"
    "${PAE_QT_ROOT}/bin/${pae_qt_dir}/Qt5Widgets${pae_qt_suffix}.dll"
)
set(pae_qt_platform "${PAE_QT_ROOT}/bin/${pae_qt_dir}/platforms/qwindows${pae_qt_suffix}.dll")
foreach(pae_runtime_file IN LISTS pae_qt_runtime_files)
    if(NOT EXISTS "${pae_runtime_file}")
        message(FATAL_ERROR "Missing whitelisted Qt runtime: ${pae_runtime_file}")
    endif()
endforeach()
if(NOT EXISTS "${pae_qt_platform}")
    message(FATAL_ERROR "Missing whitelisted Qt platform plugin: ${pae_qt_platform}")
endif()

file(REMOVE_RECURSE "${pae_deploy_dir}")
file(MAKE_DIRECTORY "${pae_deploy_dir}/platforms" "${pae_deploy_dir}/configs")
file(COPY_FILE "${PAE_UI_EXE}" "${pae_deploy_dir}/pae_protocol_lab_ui.exe" ONLY_IF_DIFFERENT)
foreach(pae_runtime_file IN LISTS pae_qt_runtime_files)
    cmake_path(GET pae_runtime_file FILENAME pae_runtime_name)
    file(COPY_FILE "${pae_runtime_file}" "${pae_deploy_dir}/${pae_runtime_name}" ONLY_IF_DIFFERENT)
endforeach()
file(COPY_FILE "${pae_qt_platform}" "${pae_deploy_dir}/platforms/qwindows${pae_qt_suffix}.dll" ONLY_IF_DIFFERENT)

string(REPLACE "|" ";" pae_config_files "${PAE_CONFIG_FILES}")
foreach(pae_config_file IN LISTS pae_config_files)
    if(NOT EXISTS "${pae_config_file}" OR NOT pae_config_file MATCHES "\\.pae\\.json$")
        message(FATAL_ERROR "Invalid whitelisted public synthetic config: ${pae_config_file}")
    endif()
    cmake_path(GET pae_config_file FILENAME pae_config_name)
    file(COPY_FILE "${pae_config_file}" "${pae_deploy_dir}/configs/${pae_config_name}" ONLY_IF_DIFFERENT)
endforeach()
