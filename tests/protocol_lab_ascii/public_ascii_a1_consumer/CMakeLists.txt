cmake_minimum_required(VERSION 3.25)
project(PaePublicAsciiA1PackageConsumer LANGUAGES CXX)

if(NOT DEFINED PAE_ASCII_ADAPTER_SOURCE_DIR)
    message(FATAL_ERROR "PAE_ASCII_ADAPTER_SOURCE_DIR must identify the Lab adapter source directory")
endif()

find_package(PAE CONFIG REQUIRED)
if(NOT TARGET PAE::pae)
    message(FATAL_ERROR "The selected SDK package did not provide PAE::pae")
endif()

add_executable(
    pae_public_ascii_a1_package_consumer
    main.cpp
    "${PAE_ASCII_ADAPTER_SOURCE_DIR}/public_ascii_offline_adapter.cpp"
    "${PAE_ASCII_ADAPTER_SOURCE_DIR}/public_ascii_offline_adapter.h"
)
target_include_directories(
    pae_public_ascii_a1_package_consumer
    PRIVATE "${PAE_ASCII_ADAPTER_SOURCE_DIR}"
)
target_compile_features(pae_public_ascii_a1_package_consumer PRIVATE cxx_std_17)
target_link_libraries(pae_public_ascii_a1_package_consumer PRIVATE PAE::pae)
