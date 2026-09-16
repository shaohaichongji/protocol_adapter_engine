cmake_minimum_required(VERSION 3.25)

project(PaeSdkStage3Consumer LANGUAGES C CXX)

if(DEFINED PAE_SOURCE_DIR)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
    add_subdirectory("${PAE_SOURCE_DIR}" pae-source)
else()
    find_package(PAE CONFIG REQUIRED)
endif()

add_executable(pae_sdk_stage3_consumer main.cpp)
target_link_libraries(pae_sdk_stage3_consumer PRIVATE PAE::pae)
target_compile_features(pae_sdk_stage3_consumer PRIVATE cxx_std_17)

get_target_property(pae_sdk_stage3_library_type PAE::pae TYPE)
if(pae_sdk_stage3_library_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(
        TARGET pae_sdk_stage3_consumer
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:PAE::pae>"
            "$<TARGET_FILE_DIR:pae_sdk_stage3_consumer>"
    )
endif()
unset(pae_sdk_stage3_library_type)
