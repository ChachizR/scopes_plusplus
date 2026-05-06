cmake_minimum_required(VERSION 3.29) # defines minimum CMake version
project(ScopesPlusPlus) # defines project name

set(CMAKE_CXX_STANDARD 23) # defines C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON) # requires the specified C++ standard
set(CMAKE_CXX_EXTENSIONS OFF) # disables compiler extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # exports compile commands for IDEs

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE AND NOT DEFINED CMAKE_CXX_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} -checks=modernize-*,readability-*) # enables clang-tidy with specific checks
endif()

set(SCPP_ENABLE_NDI_DEFAULT ON)
if(NOT WIN32)
    set(SCPP_ENABLE_NDI_DEFAULT OFF)
endif()

set(SCPP_USE_VULKAN_UI_DEFAULT OFF)
if(UNIX AND NOT APPLE)
    set(SCPP_USE_VULKAN_UI_DEFAULT ON)
endif()

option(SCPP_ENABLE_NDI "Enable NDI input support" ${SCPP_ENABLE_NDI_DEFAULT})
option(SCPP_USE_VULKAN_UI "Render the main UI with Vulkan instead of OpenGL" ${SCPP_USE_VULKAN_UI_DEFAULT})

add_subdirectory(./ext) # Add external dependencies

enable_testing()
add_subdirectory(./src/app) # Add main application source

install(TARGETS ScopesPlusPlusMain)
