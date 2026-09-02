set(
    pae_protocol_core_slice_sources
        complete_record_codec.cpp
        complete_record_codec.h
)

function(pae_configure_protocol_core_slice target_name)
    add_library(
        ${target_name}
        STATIC
            ${pae_protocol_core_slice_sources}
    )

    target_include_directories(
        ${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )

    target_link_libraries(
        ${target_name}
        PUBLIC
            pae::protocol_plan
        PRIVATE
            pae::project_options
    )

    target_compile_features(${target_name} PUBLIC cxx_std_17)

    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
endfunction()

pae_configure_protocol_core_slice(pae_protocol_core_slice)

if(PAE_BUILD_TESTING)
    pae_configure_protocol_core_slice(pae_protocol_core_slice_instrumented)
    target_compile_definitions(
        pae_protocol_core_slice_instrumented
        PUBLIC
            PAE_ENABLE_OPERATION_COUNTERS=1
    )
endif()
