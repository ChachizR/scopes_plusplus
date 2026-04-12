cmake_minimum_required(VERSION 3.29)
include(FetchContent)

FetchContent_Declare(
    opencl_c_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
    GIT_TAG v2025.07.22

    # these two tell Populate where to download into
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_headers-src
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_headers-build
)

FetchContent_Declare(
    opencl_hpp
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-CLHPP.git
    GIT_TAG v2025.07.22

    # these two tell Populate where to download into
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_hpp-src
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencl_hpp-build
)

set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OPENCL_CLHPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPENCL_HEADERS_BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(opencl_c_headers opencl_hpp)

add_library(opencl_headers INTERFACE)
target_include_directories(opencl_headers INTERFACE
    $<BUILD_INTERFACE:${opencl_c_headers_SOURCE_DIR}>
    $<BUILD_INTERFACE:${opencl_hpp_SOURCE_DIR}/include>
)
