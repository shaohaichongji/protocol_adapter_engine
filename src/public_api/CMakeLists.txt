add_library(pae_public_api compiler.cpp codec.cpp host_endpoint.cpp stream_framer.cpp)
add_library(PAE::pae ALIAS pae_public_api)

target_include_directories(
    pae_public_api
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_link_libraries(
    pae_public_api
    PRIVATE
        pae_config_compiler
        pae_protocol_core_slice
        pae_protocol_framing
        pae::protocol_plan
        pae::project_options
)
target_compile_features(pae_public_api PUBLIC cxx_std_17)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(pae_public_api PRIVATE PAE_BUILDING_LIBRARY=1)
else()
    target_compile_definitions(pae_public_api PUBLIC PAE_STATIC_DEFINE=1)
endif()
if(PAE_BUILD_TESTING)
    target_compile_definitions(
        pae_public_api
        PRIVATE
            PAE_ENABLE_PUBLIC_CODEC_TEST_HOOKS=1
            PAE_ENABLE_PUBLIC_HOST_TEST_HOOKS=1
    )
endif()

set_target_properties(
    pae_public_api
    PROPERTIES
        EXPORT_NAME pae
        OUTPUT_NAME pae
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
