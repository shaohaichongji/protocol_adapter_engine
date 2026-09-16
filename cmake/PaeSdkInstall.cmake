include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(NOT TARGET pae_public_api)
    message(FATAL_ERROR "PaeSdkInstall requires the public PAE target")
endif()

if(BUILD_SHARED_LIBS)
    set(PAE_SDK_LIBRARY_KIND SHARED)
else()
    set(PAE_SDK_LIBRARY_KIND STATIC)
endif()

install(
    TARGETS pae_public_api
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(NOT BUILD_SHARED_LIBS)
    install(
        TARGETS
            pae_config_compiler
            pae_protocol_core_slice
            pae_protocol_framing
            pae_protocol_plan
            pae_yyjson_vendor
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/pae/internal"
    )
endif()

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/pae" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(
    FILES
        "${PROJECT_SOURCE_DIR}/schema/pae.schema.json"
        "${PROJECT_SOURCE_DIR}/schema/protocol_plan_execution_semantics_v0.1.md"
        "${PROJECT_SOURCE_DIR}/schema/strict_json_profile_v0.1.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/pae/schema"
)
install(
    FILES
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_lab_exchange_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_stream_framing_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_text_slice.pae.json"
        "${PROJECT_SOURCE_DIR}/examples/config/synthetic_ascii_stream_slice.pae.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/pae/examples/config"
)
install(
    FILES
        "${PROJECT_SOURCE_DIR}/examples/public_api_sdk_consumer/CMakeLists.txt"
        "${PROJECT_SOURCE_DIR}/examples/public_api_sdk_consumer/main.cpp"
    DESTINATION "examples/sdk_consumer"
)
install(
    FILES "${PROJECT_SOURCE_DIR}/third_party/yyjson/LICENSE"
    DESTINATION "LICENSES"
    RENAME yyjson-LICENSE.txt
)

foreach(PAE_SDK_CONFIGURATION IN ITEMS Debug Release)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/PAEConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/sdk/${PAE_SDK_CONFIGURATION}/PAEConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PAE"
    )
    install(
        FILES "${PROJECT_BINARY_DIR}/sdk/${PAE_SDK_CONFIGURATION}/PAEConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PAE"
        CONFIGURATIONS "${PAE_SDK_CONFIGURATION}"
    )
endforeach()
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/sdk/PAEConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(
    FILES "${PROJECT_BINARY_DIR}/sdk/PAEConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PAE"
)

unset(PAE_SDK_CONFIGURATION)
unset(PAE_SDK_LIBRARY_KIND)
