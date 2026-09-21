function(pae_lab_configure_deployment target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "Unknown Lab deployment target: ${target}")
  endif()
  set(config_names
    synthetic_ui_v05.pae.json synthetic_ui_v06.pae.json synthetic_ui_v07.pae.json
    synthetic_ui_v08.pae.json synthetic_ui_inspect_multi_v05.pae.json
    synthetic_binary_ui_stage1.pae.json synthetic_ascii_decode_only.pae.json
    synthetic_ascii_encode_only.pae.json synthetic_ascii_text_slice.pae.json
    synthetic_ascii_literal_only.pae.json synthetic_ascii_stream_slice.pae.json
    synthetic_stream_framing_slice.pae.json
    synthetic_ui_max.pae.json)
  set(config_files)
  foreach(name IN LISTS config_names)
    set(path "${PAE_LAB_CONFIG_ROOT}/${name}")
    if(NOT EXISTS "${path}")
      message(FATAL_ERROR "Missing standalone Lab config: ${path}")
    endif()
    list(APPEND config_files "${path}")
  endforeach()
  string(JOIN "|" config_arg ${config_files})
  set_target_properties("${target}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  add_custom_command(TARGET "${target}" POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DPAE_UI_EXE=$<TARGET_FILE:${target}>"
      "-DPAE_LIBRARY_KIND=${PAE_LIBRARY_KIND}"
      "-DPAE_RUNTIME_DLL=${normalized_pae_runtime_dll}"
      "-DPAE_RUNTIME_SHA256=${pae_runtime_sha256}"
      "-DPAE_QT_ROOT=${PAE_QT_ROOT}"
      "-DPAE_BUILD_CONFIG=$<CONFIG>"
      "-DPAE_DEPLOY_DIR=${PAE_LAB_DEPLOY_ROOT}/$<CONFIG>"
      "-DPAE_ALLOWED_OUT_ROOT=${PAE_LAB_DEPLOY_ROOT}"
      "-DPAE_CONFIG_FILES=${config_arg}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/DeployPaeLabRun.cmake"
    VERBATIM)
endfunction()
