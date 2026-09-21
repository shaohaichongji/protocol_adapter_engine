cmake_minimum_required(VERSION 3.25)

project(PaeGettingStarted LANGUAGES CXX)

if(DEFINED PAE_SOURCE_DIR)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
    add_subdirectory("${PAE_SOURCE_DIR}" pae-source)
else()
    find_package(PAE CONFIG REQUIRED)
endif()

add_executable(pae_getting_started main.cpp)
target_link_libraries(pae_getting_started PRIVATE PAE::pae)
target_compile_features(pae_getting_started PRIVATE cxx_std_17)

get_target_property(pae_getting_started_library_type PAE::pae TYPE)
if(pae_getting_started_library_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(
        TARGET pae_getting_started
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:PAE::pae>"
            "$<TARGET_FILE_DIR:pae_getting_started>"
    )
endif()
unset(pae_getting_started_library_type)
