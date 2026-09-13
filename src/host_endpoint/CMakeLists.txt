add_library(pae_host_endpoint STATIC host_endpoint.cpp host_endpoint.h)
target_include_directories(pae_host_endpoint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pae_host_endpoint PUBLIC pae_protocol_core_slice pae_protocol_framing
    PRIVATE pae::project_options)
target_compile_features(pae_host_endpoint PUBLIC cxx_std_17)
