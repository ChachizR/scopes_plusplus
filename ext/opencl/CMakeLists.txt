include(FetchContent)

FetchContent_Declare(
    opencl_hpp
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-CLHPP.git
    GIT_TAG v2025.07.22
)

FetchContent_MakeAvailable(opencl_hpp)

add_library(opencl_headers INTERFACE)

target_include_directories(opencl_headers INTERFACE
    $<BUILD_INTERFACE:${opencl_hpp_SOURCE_DIR}/include>
)