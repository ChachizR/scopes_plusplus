Include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
)

FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "./*.cpp" "./*.hpp")

add_executable(tests ${TEST_SOURCES})
scpp_enable_clang_tidy(tests)

target_link_libraries(
    tests PRIVATE
    $<TARGET_PROPERTY:ScopesPlusPlusMain,LINK_LIBRARIES>
    Catch2::Catch2WithMain
)

target_include_directories(
    tests PRIVATE
    $<TARGET_PROPERTY:${PROJECT_NAME},INCLUDE_DIRECTORIES>
)

target_compile_definitions(
    tests PUBLIC
    CL_HPP_MINIMUM_OPENCL_VERSION=120
)

if(APPLE)
    target_compile_definitions(tests PUBLIC
        CL_TARGET_OPENCL_VERSION=120
        CL_HPP_TARGET_OPENCL_VERSION=120
    )
else()
    target_compile_definitions(tests PUBLIC
        CL_TARGET_OPENCL_VERSION=300
        CL_HPP_TARGET_OPENCL_VERSION=300
    )
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CTest)
include(Catch)
catch_discover_tests(tests)
