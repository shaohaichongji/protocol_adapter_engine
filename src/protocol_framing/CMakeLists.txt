add_library(
    pae_protocol_framing
    STATIC
        stream_framer.cpp
        stream_framer.h
)
add_library(pae::protocol_framing ALIAS pae_protocol_framing)

target_include_directories(pae_protocol_framing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pae_protocol_framing PUBLIC pae::protocol_plan PRIVATE pae::project_options)
target_compile_features(pae_protocol_framing PUBLIC cxx_std_17)

set_target_properties(
    pae_protocol_framing
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
