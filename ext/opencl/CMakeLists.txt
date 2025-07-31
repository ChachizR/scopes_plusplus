cmake_minimum_required(VERSION 3.29)
include(FetchContent)

FetchContent_Declare(
    opencl_hpp
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-CLHPP.git
    GIT_TAG v2025.07.22

    # these two tell Populate where to download into
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_hpp-src
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_hpp-build
)

FetchContent_GetProperties(opencl_hpp)

if(NOT opencl_hpp_POPULATED)
    FetchContent_Populate(opencl_hpp)
endif()

add_library(opencl_headers INTERFACE)
target_include_directories(opencl_headers INTERFACE
    $<BUILD_INTERFACE:${opencl_hpp_SOURCE_DIR}/include>
)