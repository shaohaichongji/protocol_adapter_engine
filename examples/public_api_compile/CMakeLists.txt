cmake_minimum_required(VERSION 3.25)

project(PaePublicCompileConsumer LANGUAGES C CXX)

if(NOT DEFINED PAE_SOURCE_DIR)
    message(FATAL_ERROR "Set PAE_SOURCE_DIR to the ProtocolAdapterEngine source directory.")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)

add_executable(pae_public_compile_consumer main.cpp)
target_link_libraries(pae_public_compile_consumer PRIVATE PAE::pae)
